// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "SessionController.h"
#include "common/Settings.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QImageReader>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QUdpSocket>
#include <QUuid>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace deskflow::gui::session {
using professional::Message;
namespace {
bool inputFeature(const QString &feature)
{
  return feature == "bidirectional-control" || feature == "simultaneous-input";
}
bool integer(const QJsonObject &body, const char *key, qint64 min, qint64 max, qint64 &out)
{
  const auto value = body.value(QLatin1String(key));
  const double number = value.toDouble(std::nan(""));
  if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number || number < min || number > max)
    return false;
  out = static_cast<qint64>(number);
  return true;
}
QByteArray binary(const QJsonObject &body, qsizetype limit)
{
  if (!body.value("data").isString())
    return {};
  const auto encoded = body.value("data").toString().toLatin1();
  if (encoded.size() > limit * 4 / 3 + 4)
    return {};
  const auto decoded = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
  return decoded.size() <= limit && decoded.toBase64() == encoded ? decoded : QByteArray{};
}
QString peerStorage(const QString &peer)
{
  return "Professional/Peer/" +
         QString::fromLatin1(QCryptographicHash::hash(peer.toUtf8(), QCryptographicHash::Sha256).toHex());
}
} // namespace

bool FrameAssembly::append(quint32 frame, qsizetype total, qsizetype offset, const QByteArray &chunk)
{
  if (!frame || total <= 0 || total > professional::kMaxFrameBytes || offset < 0 || chunk.isEmpty() ||
      chunk.size() > professional::kChunkBytes || offset > total || chunk.size() > total - offset)
    return false;
  if (offset == 0) {
    if (frame <= m_frame)
      return false;
    m_frame = frame;
    m_total = total;
    m_bytes.clear();
  }
  if (frame != m_frame || total != m_total || offset != m_bytes.size())
    return false;
  m_bytes.append(chunk);
  return true;
}
QByteArray FrameAssembly::take()
{
  if (!complete())
    return {};
  m_total = 0;
  return std::exchange(m_bytes, {});
}

SessionController::SessionController(Transport transport, std::unique_ptr<SessionPlatform> platform, QObject *parent)
    : QObject(parent),
      m_transport(std::move(transport)),
      m_platform(std::move(platform))
{
  if (!m_transport.authorized)
    m_transport.authorized = [](const QString &feature) { return Settings::hasPlanFeature(feature); };
  if (!m_transport.now)
    m_transport.now = [] {
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count();
    };
  connect(&m_tick, &QTimer::timeout, this, &SessionController::tick);
  connect(&m_video, &QTimer::timeout, this, &SessionController::captureFrame);
  m_video.setInterval(200);
  m_motionTimer.setSingleShot(true);
  m_motionTimer.setInterval(8);
  connect(&m_motionTimer, &QTimer::timeout, this, &SessionController::flushMotion);
  connect(m_platform.get(), &SessionPlatform::audioCaptured, this, &SessionController::sendAudio);
  connect(m_platform.get(), &SessionPlatform::inputCaptured, this, &SessionController::handleInput);
  connect(m_platform.get(), &SessionPlatform::failure, this, [this](const QString &feature, const QString &message) {
    const auto sessions = m_sessions;
    for (const auto &session : sessions)
      if (session.feature == feature || (inputFeature(feature) && inputFeature(session.feature)))
        closeSession(session.id, true);
    Q_EMIT status(message);
  });
}
SessionController::~SessionController()
{
  stopAll();
}
qint64 SessionController::now() const
{
  return m_transport.now();
}
bool SessionController::permitted(const QString &feature) const
{
  return m_transport.authorized(feature);
}
int SessionController::activeCount() const
{
  return static_cast<int>(std::count_if(m_sessions.cbegin(), m_sessions.cend(), [](const Session &s) {
    return s.active;
  }));
}
QStringList SessionController::rememberedPeers() const
{
  return Settings::value("Professional/KnownPeers").toStringList();
}

void SessionController::setOnline(bool online, const QString &localName)
{
  if (!online || m_local != localName)
    stopAll();
  m_local = localName;
  m_online = online;
  if (!online) {
    m_tick.stop();
    m_lastTick = 0;
    m_peers.clear();
    Q_EMIT peersChanged(m_peers);
  } else {
    m_tick.start(m_sessions.isEmpty() ? 3000 : 1000);
    m_lastDiscovery = -10000;
    tick();
  }
}

QString SessionController::request(const QString &peer, const QString &feature)
{
  if (!m_online || !m_peers.contains(peer)) {
    Q_EMIT status(tr("Connect both computers with the current Ieum version first."));
    return {};
  }
  if (!permitted(feature)) {
    Q_EMIT status(tr("This feature requires an active matching plan."));
    return {};
  }
  if (m_sessions.size() >= 12) {
    Q_EMIT status(tr("Finish an existing sharing session first."));
    return {};
  }
  for (const auto &existing : m_sessions) {
    if (existing.peer == peer && existing.feature == feature)
      return existing.id;
    if (inputFeature(feature) && inputFeature(existing.feature) &&
        (!existing.outgoing || existing.feature != feature || feature == "bidirectional-control")) {
      Q_EMIT status(tr("Stop the current input session before switching control."));
      return {};
    }
    if (feature == "audio-sharing" && existing.feature == feature) {
      Q_EMIT status(tr("Receive one audio source at a time. Stop other audio sessions first."));
      return {};
    }
  }
  Session session;
  session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  session.peer = peer;
  session.feature = feature;
  session.outgoing = true;
  session.expires = now() + 60000;
  const auto id = session.id;
  m_sessions.insert(id, session);
  m_tick.setInterval(1000);
  if (!send(m_sessions[id], "open")) {
    closeSession(id, false);
    Q_EMIT status(tr("The sharing request could not be sent."));
    return {};
  }
  Q_EMIT status(tr("Waiting for %1 to allow sharing…").arg(peer));
  return id;
}

bool SessionController::send(Session &session, const QString &operation, const QJsonObject &body)
{
  return m_online && m_transport.send &&
         m_transport.send(professional::encode({{}, session.peer, operation, session.feature, session.id, body}));
}

QString SessionController::trustKey(const QString &peer, const QString &feature) const
{
  return peerStorage(peer) + "/Allow/" + feature;
}
bool SessionController::trusted(const QString &peer, const QString &feature, const QString &connection) const
{
  const auto generation = Settings::value("Professional/PermissionGeneration").toString();
  return !generation.isEmpty() && !connection.isEmpty() &&
         Settings::value(trustKey(peer, feature)).toString() == generation + ":" + connection;
}
void SessionController::consent(const QString &id, bool allow, bool remember)
{
  auto found = m_sessions.find(id);
  if (found == m_sessions.end() || found->outgoing || found->active)
    return;
  if (!allow || !permitted(found->feature)) {
    closeSession(id, true);
    return;
  }
  if (remember) {
    auto generation = Settings::value("Professional/PermissionGeneration").toString();
    if (generation.isEmpty()) {
      generation = QUuid::createUuid().toString(QUuid::WithoutBraces);
      Settings::setValue("Professional/PermissionGeneration", generation);
    }
    Settings::setValue(trustKey(found->peer, found->feature), generation + ":" + found->peerConnection);
  }
  acceptSession(id);
}

void SessionController::acceptSession(const QString &id)
{
  auto found = m_sessions.find(id);
  if (found == m_sessions.end())
    return;
  if (found->feature == "remote-viewing" && !m_platform->requestScreenPermission()) {
    Q_EMIT status(m_platform->lastError());
    closeSession(id, true);
    return;
  }
  for (const auto &other : m_sessions) {
    if (other.id == id || !other.active)
      continue;
    if ((inputFeature(found->feature) && inputFeature(other.feature)) ||
        (found->feature == "audio-sharing" && other.feature == "audio-sharing" && other.outgoing)) {
      Q_EMIT status(tr("This computer already has an input owner or an incompatible audio session."));
      closeSession(id, true);
      return;
    }
  }
  found->active = true;
  found->expires = now() + professional::kSessionTimeoutMs;
  const auto session = *found;
  refreshPlatform();
  found = m_sessions.find(id);
  if (found == m_sessions.end())
    return;
  if (!send(*found, "accept")) {
    closeSession(id, false);
    return;
  }
  Q_EMIT sessionChanged(id, session.peer, session.feature, true);
  Q_EMIT status(tr("Sharing with %1. You can stop it at any time.").arg(session.peer));
}

void SessionController::receive(const QByteArray &bytes)
{
  const auto parsed = professional::decode(bytes);
  if (!parsed || !m_online || parsed->target != m_local || !professional::validPeer(parsed->source))
    return;
  const auto &message = *parsed;
  if (message.operation == "peers") {
    const auto peers = message.body.value("peers").toArray();
    if (peers.size() > 64)
      return;
    QStringList next;
    for (const auto &peer : peers) {
      if (!peer.isString() || !professional::validPeer(peer.toString()) || peer.toString() == m_local)
        return;
      if (!next.contains(peer.toString()))
        next.append(peer.toString());
    }
    const auto old = m_peers;
    m_peers = next;
    const auto sessions = m_sessions;
    for (const auto &session : sessions)
      if (!next.contains(session.peer))
        closeSession(session.id, false);
    if (old != next)
      Q_EMIT peersChanged(next);
    for (const auto &peer : next)
      if (!old.contains(peer))
        announce(peer);
    return;
  }
  if (!m_peers.contains(message.source)) {
    m_lastDiscovery = -10000;
    return;
  }
  if (message.operation == "hello") {
    rememberPeer(message.source, message.body);
    if (message.body.value("reply") == QJsonValue(false))
      announce(message.source, true);
    return;
  }
  if (message.operation == "open") {
    const auto connection = message.body.value("connection").toString();
    const auto id = QUuid::fromString(connection);
    if (id.isNull() || id.toString(QUuid::WithoutBraces) != connection)
      return;
    if (!permitted(message.feature) || m_sessions.size() >= 12 || m_platform->locked()) {
      Session rejected;
      rejected.id = message.session;
      rejected.peer = message.source;
      rejected.feature = message.feature;
      (void)send(rejected, "close");
      return;
    }
    if (m_sessions.contains(message.session))
      return;
    for (const auto &other : m_sessions)
      if (other.peer == message.source && other.feature == message.feature)
        return;
    Session session;
    session.id = message.session;
    session.peer = message.source;
    session.feature = message.feature;
    session.peerConnection = connection;
    session.expires = now() + 60000;
    m_sessions.insert(session.id, session);
    m_tick.setInterval(1000);
    if (trusted(session.peer, session.feature, session.peerConnection))
      consent(session.id, true);
    else
      Q_EMIT consentRequested(session.id, session.peer, session.feature);
    return;
  }
  auto found = m_sessions.find(message.session);
  if (found == m_sessions.end() || found->peer != message.source || found->feature != message.feature)
    return;
  if (message.operation == "close") {
    closeSession(message.session, false);
    return;
  }
  if (!permitted(found->feature)) {
    closeSession(message.session, true);
    return;
  }
  if (message.operation == "accept") {
    if (!found->outgoing || found->active)
      return;
    found->active = true;
    found->expires = now() + professional::kSessionTimeoutMs;
    const auto session = *found;
    refreshPlatform();
    if (m_sessions.contains(message.session))
      Q_EMIT sessionChanged(session.id, session.peer, session.feature, true);
    return;
  }
  if (!found->active)
    return;
  found->expires = now() + professional::kSessionTimeoutMs;
  if (message.operation == "ping")
    return;
  if (message.operation == "input" && !found->outgoing && inputFeature(found->feature)) {
    qint64 sequence = 0;
    const auto input = Input::fromJson(message.body.value("event").toObject());
    if (message.body.size() != 2 || !integer(message.body, "seq", 1, 0xffffffff, sequence) ||
        sequence != static_cast<qint64>(found->received) + 1 || !input || m_platform->locked()) {
      closeSession(message.session, true);
      return;
    }
    found->received = static_cast<quint32>(sequence);
    if (!m_platform->inject(*input)) {
      Q_EMIT status(m_platform->lastError());
      closeSession(message.session, true);
    }
    return;
  }
  if (message.operation == "frame" && found->outgoing && found->feature == "remote-viewing") {
    qint64 frame = 0, total = 0, offset = 0;
    const auto data = binary(message.body, professional::kChunkBytes);
    if (message.body.size() != 4 || !integer(message.body, "frame", 1, 0xffffffff, frame) ||
        !integer(message.body, "total", 1, professional::kMaxFrameBytes, total) ||
        !integer(message.body, "offset", 0, total, offset) ||
        !found->assembly.append(static_cast<quint32>(frame), total, offset, data)) {
      closeSession(message.session, true);
      return;
    }
    (void)send(
        *found, "ack", {{"frame", static_cast<double>(frame)}, {"offset", static_cast<double>(found->assembly.size())}}
    );
    if (found->assembly.complete()) {
      auto jpeg = found->assembly.take();
      QBuffer buffer(&jpeg);
      buffer.open(QIODevice::ReadOnly);
      QImageReader reader(&buffer, "jpeg");
      const auto size = reader.size();
      if (!size.isValid() || size.width() > 1920 || size.height() > 1080 || reader.format() != "jpeg") {
        closeSession(message.session, true);
        return;
      }
      const auto image = reader.read();
      if (image.isNull()) {
        closeSession(message.session, true);
        return;
      }
      Q_EMIT frameReceived(message.session, message.source, image);
    }
    return;
  }
  if (message.operation == "ack" && !found->outgoing && found->feature == "remote-viewing") {
    qint64 frame = 0, offset = 0;
    if (!integer(message.body, "frame", 1, 0xffffffff, frame) ||
        !integer(message.body, "offset", 1, professional::kMaxFrameBytes, offset) || frame != found->frameId ||
        offset != found->expectedAck || found->frame.isEmpty()) {
      closeSession(message.session, true);
      return;
    }
    found->offset = offset;
    found->ackDeadline = 0;
    if (offset == found->frame.size())
      found->frame.clear();
    else
      sendFrameChunk(*found);
    return;
  }
  if (message.operation == "audio" && found->outgoing && found->feature == "audio-sharing") {
    qint64 sequence = 0;
    const auto pcm = binary(message.body, professional::kChunkBytes);
    if (!integer(message.body, "seq", 1, 0xffffffff, sequence) || sequence <= found->received || pcm.isEmpty() ||
        pcm.size() % 4) {
      closeSession(message.session, true);
      return;
    }
    found->received = sequence;
    if (!m_platform->playAudio(pcm)) {
      Q_EMIT status(m_platform->lastError());
      closeSession(message.session, true);
      return;
    }
    (void)send(*found, "ack", {{"seq", static_cast<double>(sequence)}});
    return;
  }
  if (message.operation == "ack" && !found->outgoing && found->feature == "audio-sharing") {
    qint64 sequence = 0;
    if (!integer(message.body, "seq", 1, found->sequence, sequence) || sequence <= found->audioAck) {
      closeSession(message.session, true);
      return;
    }
    found->audioAck = sequence;
    found->ackDeadline = found->audioAck == found->sequence ? 0 : now() + 3000;
    return;
  }
  if (found->feature == "coordinated-lock-wake" && (message.operation == "lock" || message.operation == "wake")) {
    if (message.operation == "lock") {
      if (m_platform->lockDesktop())
        m_lastLocked = true;
      else
        Q_EMIT status(m_platform->lastError());
    } else if (!m_platform->wakeDisplays())
      Q_EMIT status(m_platform->lastError());
    return;
  }
  closeSession(message.session, true);
}

void SessionController::closeSession(QString id, bool notify)
{
  const auto found = m_sessions.find(id);
  if (found == m_sessions.end())
    return;
  auto session = *found;
  m_sessions.erase(found);
  if (m_sessions.isEmpty())
    m_tick.setInterval(3000);
  if (notify)
    (void)send(session, "close");
  if (!session.outgoing && inputFeature(session.feature))
    m_platform->releaseInput();
  Q_EMIT sessionChanged(id, session.peer, session.feature, false);
  refreshPlatform();
}
void SessionController::stop(const QString &id)
{
  closeSession(id, true);
}
void SessionController::stopAll()
{
  const auto ids = m_sessions.keys();
  for (const auto &id : ids)
    closeSession(id, true);
  m_platform->releaseInput();
}
void SessionController::revokePermissions()
{
  stopAll();
  Settings::setValue("Professional/PermissionGeneration", QUuid::createUuid().toString(QUuid::WithoutBraces));
  Q_EMIT status(tr("Saved sharing permissions were revoked. Each computer must ask again."));
}

void SessionController::refreshPlatform()
{
  bool audio = false, video = false, playback = false;
  std::optional<SessionPlatform::InputMode> inputMode;
  for (const auto &session : m_sessions) {
    if (!session.active)
      continue;
    if (session.feature == "remote-viewing" && !session.outgoing)
      video = true;
    if (session.feature == "audio-sharing") {
      if (session.outgoing)
        playback = true;
      else
        audio = true;
    }
    if (inputFeature(session.feature))
      inputMode = session.outgoing ? (session.feature == "simultaneous-input" ? SessionPlatform::InputMode::Broadcast
                                                                              : SessionPlatform::InputMode::Control)
                                   : SessionPlatform::InputMode::Observe;
  }
  if (video) {
    if (!m_video.isActive())
      m_video.start();
  } else
    m_video.stop();
  if (!playback)
    m_platform->stopAudioPlayback();
  if (audio != m_audioCapture) {
    m_audioCapture = audio;
    if (audio && !m_platform->startAudioCapture()) {
      const auto ids = m_sessions.keys();
      for (const auto &id : ids)
        if (m_sessions.value(id).feature == "audio-sharing" && !m_sessions.value(id).outgoing)
          closeSession(id, true);
      Q_EMIT status(m_platform->lastError());
    } else if (!audio)
      m_platform->stopAudioCapture();
  }
  if (inputMode != m_captureMode) {
    m_platform->stopInputCapture();
    m_motion.reset();
    m_motionTimer.stop();
    m_captureMode = inputMode;
    m_inputTransition = inputMode.has_value();
    m_inputReadyDeadline = now() + 3000;
    if (m_transport.localInputMode)
      m_transport.localInputMode(inputMode.has_value());
  }
}

void SessionController::inputModeReady(bool active)
{
  if (!m_inputTransition || !m_captureMode)
    return;
  m_inputTransition = false;
  if (!active || !m_platform->startInputCapture(*m_captureMode)) {
    Q_EMIT status(active ? m_platform->lastError() : tr("Input sharing could not take ownership. Restart sharing."));
    const auto ids = m_sessions.keys();
    for (const auto &id : ids)
      if (inputFeature(m_sessions.value(id).feature))
        closeSession(id, true);
  }
}

void SessionController::captureFrame()
{
  bool wanted = false;
  for (const auto &session : m_sessions)
    if (session.active && !session.outgoing && session.feature == "remote-viewing" && session.frame.isEmpty())
      wanted = true;
  if (!wanted)
    return;
  auto image = m_platform->screen();
  if (image.isNull()) {
    Q_EMIT status(tr("Screen capture is unavailable or the desktop is locked."));
    return;
  }
  if (image.width() > 1280 || image.height() > 720)
    image = image.scaled(1280, 720, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QByteArray jpeg;
  QBuffer output(&jpeg);
  output.open(QIODevice::WriteOnly);
  if (!image.save(&output, "JPEG", 55) || jpeg.size() > professional::kMaxFrameBytes)
    return;
  if (++m_frameId == 0)
    ++m_frameId;
  const auto ids = m_sessions.keys();
  for (const auto &id : ids) {
    auto found = m_sessions.find(id);
    if (found != m_sessions.end() && found->active && !found->outgoing && found->feature == "remote-viewing" &&
        found->frame.isEmpty()) {
      found->frame = jpeg;
      found->frameId = m_frameId;
      found->offset = 0;
      sendFrameChunk(*found);
    }
  }
}
void SessionController::sendFrameChunk(Session &session)
{
  const auto chunk = session.frame.mid(session.offset, professional::kChunkBytes);
  session.expectedAck = session.offset + chunk.size();
  session.ackDeadline = now() + 3000;
  if (!send(
          session, "frame",
          {{"frame", static_cast<double>(session.frameId)},
           {"total", static_cast<double>(session.frame.size())},
           {"offset", static_cast<double>(session.offset)},
           {"data", QString::fromLatin1(chunk.toBase64())}}
      ))
    closeSession(session.id, true);
}
void SessionController::sendAudio(const QByteArray &pcm)
{
  if (!m_audioCapture || pcm.isEmpty() || pcm.size() > professional::kChunkBytes || pcm.size() % 4)
    return;
  const auto ids = m_sessions.keys();
  for (const auto &id : ids) {
    auto found = m_sessions.find(id);
    if (found != m_sessions.end() && found->active && !found->outgoing && found->feature == "audio-sharing" &&
        found->sequence - found->audioAck < 4) {
      if (!found->ackDeadline)
        found->ackDeadline = now() + 3000;
      if (++found->sequence == 0 ||
          !send(
              *found, "audio",
              {{"seq", static_cast<double>(found->sequence)}, {"data", QString::fromLatin1(pcm.toBase64())}}
          ))
        closeSession(id, true);
    }
  }
}

void SessionController::handleInput(const Input &input)
{
  if (input.kind == Input::Kind::Escape || m_captureMode == SessionPlatform::InputMode::Observe) {
    const auto ids = m_sessions.keys();
    for (const auto &id : ids)
      if (inputFeature(m_sessions.value(id).feature))
        closeSession(id, true);
    Q_EMIT status(tr("Input control returned to this computer."));
    return;
  }
  if (input.kind == Input::Kind::Move) {
    m_motion = input;
    if (!m_motionTimer.isActive())
      m_motionTimer.start();
    return;
  }
  flushMotion();
  sendInput(input);
}
void SessionController::flushMotion()
{
  if (!m_motion)
    return;
  const auto input = *m_motion;
  m_motion.reset();
  sendInput(input);
}
void SessionController::sendInput(const Input &input)
{
  const auto ids = m_sessions.keys();
  for (const auto &id : ids) {
    auto found = m_sessions.find(id);
    if (found != m_sessions.end() && found->active && found->outgoing && inputFeature(found->feature))
      if (++found->sequence == 0 ||
          !send(*found, "input", {{"seq", static_cast<double>(found->sequence)}, {"event", input.toJson()}}))
        closeSession(id, true);
  }
}

void SessionController::tick()
{
  const auto time = now();
  if (!m_online)
    return;
  if (m_lastTick && time - m_lastTick > professional::kSessionTimeoutMs)
    stopAll();
  m_lastTick = time;
  if (time - m_lastDiscovery >= 3000) {
    m_lastDiscovery = time;
    if (m_transport.send)
      (void)m_transport.send(professional::encode({{}, {}, "discover", {}, {}, {}}));
  }
  const bool isLocked = !m_sessions.isEmpty() && m_platform->locked();
  const auto ids = m_sessions.keys();
  for (const auto &id : ids) {
    auto found = m_sessions.find(id);
    if (found == m_sessions.end())
      continue;
    if (found->expires <= time || !permitted(found->feature) || (found->ackDeadline && found->ackDeadline <= time)) {
      closeSession(id, true);
      continue;
    }
    if (!found->active)
      continue;
    if (inputFeature(found->feature) && m_inputTransition && m_inputReadyDeadline <= time) {
      closeSession(id, true);
      continue;
    }
    if (isLocked && found->feature != "coordinated-lock-wake") {
      closeSession(id, true);
      continue;
    }
    if (found->feature == "coordinated-lock-wake" && isLocked != m_lastLocked)
      (void)send(*found, isLocked ? "lock" : "wake");
    (void)send(*found, "ping");
  }
  m_lastLocked = isLocked;
  if (m_captureMode && m_transport.localInputMode)
    m_transport.localInputMode(true);
}

void SessionController::announce(const QString &peer, bool reply)
{
  if (!permitted("coordinated-lock-wake"))
    return;
  QJsonArray macs;
  for (const auto &network : QNetworkInterface::allInterfaces()) {
    if (!(network.flags() & QNetworkInterface::IsUp) || (network.flags() & QNetworkInterface::IsLoopBack))
      continue;
    const auto mac = network.hardwareAddress().toUpper();
    if (QRegularExpression("^(?:[0-9A-F]{2}:){5}[0-9A-F]{2}$").match(mac).hasMatch() && mac != "00:00:00:00:00:00" &&
        macs.size() < 8)
      macs.append(mac);
  }
  Session hello;
  hello.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  hello.peer = peer;
  hello.feature = "coordinated-lock-wake";
  (void)send(hello, "hello", {{"macs", macs}, {"reply", reply}});
}
void SessionController::rememberPeer(const QString &peer, const QJsonObject &body)
{
  if (!permitted("coordinated-lock-wake") || !body.value("macs").isArray())
    return;
  const auto macs = body.value("macs").toArray();
  if (macs.size() > 8)
    return;
  QStringList values;
  for (const auto &value : macs) {
    const auto mac = value.toString();
    if (!QRegularExpression("^(?:[0-9A-F]{2}:){5}[0-9A-F]{2}$").match(mac).hasMatch() || mac == "00:00:00:00:00:00")
      return;
    values.append(mac);
  }
  Settings::setValue(peerStorage(peer) + "/Macs", values);
  auto peers = rememberedPeers();
  if (!peers.contains(peer) && peers.size() < 64) {
    peers.append(peer);
    Settings::setValue("Professional/KnownPeers", peers);
  }
}
bool SessionController::wake(const QString &peer)
{
  if (!permitted("coordinated-lock-wake")) {
    Q_EMIT status(tr("Wake requires Professional Max."));
    return false;
  }
  bool sent = false;
  for (auto &session : m_sessions)
    if (session.active && session.peer == peer && session.feature == "coordinated-lock-wake")
      sent = send(session, "wake") || sent;
  QUdpSocket socket;
  const auto macs = Settings::value(peerStorage(peer) + "/Macs").toStringList();
  for (const auto &text : macs) {
    auto compact = text;
    compact.remove(':');
    const auto mac = QByteArray::fromHex(compact.toLatin1());
    if (mac.size() != 6)
      continue;
    QByteArray packet(6, static_cast<char>(0xff));
    for (int i = 0; i < 16; ++i)
      packet.append(mac);
    for (const auto &network : QNetworkInterface::allInterfaces()) {
      if (!(network.flags() & QNetworkInterface::IsUp) || !(network.flags() & QNetworkInterface::CanBroadcast))
        continue;
      for (const auto &address : network.addressEntries())
        if (address.broadcast().protocol() == QAbstractSocket::IPv4Protocol)
          sent = (socket.writeDatagram(packet, address.broadcast(), 9) == packet.size()) || sent;
    }
  }
  Q_EMIT status(
      sent ? tr("Wake request sent. Sleep wake needs Wake-on-LAN enabled in the computer and network adapter.")
           : tr("Connect this computer once to learn its network address, or start lock/wake sharing.")
  );
  return sent;
}
void SessionController::lockTogether()
{
  if (!permitted("coordinated-lock-wake"))
    return;
  bool sent = false;
  for (auto &session : m_sessions)
    if (session.active && session.feature == "coordinated-lock-wake")
      sent = send(session, "lock") || sent;
  if (sent) {
    if (m_platform->lockDesktop())
      m_lastLocked = true;
    else
      Q_EMIT status(m_platform->lastError());
  } else
    Q_EMIT status(tr("Start lock/wake sharing with a computer first."));
}
} // namespace deskflow::gui::session
