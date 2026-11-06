// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#pragma once
#include "SessionPlatform.h"
#include "common/ProfessionalMessage.h"
#include <QHash>
#include <QTimer>
#include <functional>

namespace deskflow::gui::session {
class FrameAssembly
{
public:
  bool append(quint32 frame, qsizetype total, qsizetype offset, const QByteArray &chunk);
  bool complete() const
  {
    return m_total > 0 && m_bytes.size() == m_total;
  }
  QByteArray take();
  qsizetype size() const
  {
    return m_bytes.size();
  }

private:
  quint32 m_frame = 0;
  qsizetype m_total = 0;
  QByteArray m_bytes;
};

class SessionController final : public QObject
{
  Q_OBJECT
public:
  struct Transport
  {
    std::function<bool(const QByteArray &)> send;
    std::function<void(bool)> localInputMode;
    std::function<bool(const QString &)> authorized;
    std::function<qint64()> now;
  };
  SessionController(Transport transport, std::unique_ptr<SessionPlatform> platform, QObject *parent = nullptr);
  ~SessionController() override;
  void setOnline(bool online, const QString &localName);
  QStringList peers() const
  {
    return m_peers;
  }
  QStringList rememberedPeers() const;
  QString request(const QString &peer, const QString &feature);
  void consent(const QString &session, bool allow, bool remember = false);
  void receive(const QByteArray &bytes);
  void stop(const QString &session);
  void stopAll();
  void revokePermissions();
  bool wake(const QString &peer);
  void lockTogether();
  void tick();
  void inputModeReady(bool active);
  int activeCount() const;
  bool permitted(const QString &feature) const;
Q_SIGNALS:
  void peersChanged(const QStringList &peers);
  void consentRequested(const QString &session, const QString &peer, const QString &feature);
  void sessionChanged(const QString &session, const QString &peer, const QString &feature, bool active);
  void frameReceived(const QString &session, const QString &peer, const QImage &image);
  void status(const QString &message);

private:
  struct Session
  {
    QString id, peer, feature, peerConnection;
    bool outgoing = false, active = false;
    qint64 expires = 0, ackDeadline = 0;
    quint32 sequence = 0, received = 0, audioAck = 0;
    QByteArray frame;
    qsizetype offset = 0, expectedAck = 0;
    quint32 frameId = 0;
    FrameAssembly assembly;
  };
  qint64 now() const;
  bool send(Session &session, const QString &operation, const QJsonObject &body = {});
  void closeSession(QString id, bool notify);
  void refreshPlatform();
  void acceptSession(const QString &id);
  void captureFrame();
  void sendFrameChunk(Session &session);
  void sendAudio(const QByteArray &pcm);
  void handleInput(const Input &input);
  void flushMotion();
  void sendInput(const Input &input);
  void rememberPeer(const QString &peer, const QJsonObject &body);
  void announce(const QString &peer, bool reply = false);
  bool trusted(const QString &peer, const QString &feature, const QString &connection) const;
  QString trustKey(const QString &peer, const QString &feature) const;
  Transport m_transport;
  std::unique_ptr<SessionPlatform> m_platform;
  QHash<QString, Session> m_sessions;
  QStringList m_peers;
  QString m_local;
  QTimer m_tick, m_video, m_motionTimer;
  std::optional<Input> m_motion;
  std::optional<SessionPlatform::InputMode> m_captureMode;
  bool m_online = false, m_audioCapture = false, m_inputTransition = false, m_lastLocked = false;
  qint64 m_lastDiscovery = -10000, m_lastTick = 0;
  qint64 m_inputReadyDeadline = 0;
  quint32 m_frameId = 0;
};
} // namespace deskflow::gui::session
