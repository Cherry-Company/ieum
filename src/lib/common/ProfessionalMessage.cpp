// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "ProfessionalMessage.h"
#include "common/Settings.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

namespace deskflow::professional {
bool validPeer(const QString &name)
{
  if (name.isEmpty() || name.toUtf8().size() > 128)
    return false;
  for (const auto character : name)
    if (character.isNull() || character.category() == QChar::Other_Control)
      return false;
  return true;
}

bool validMessage(const Message &message)
{
  static const QSet<QString> operations = {"discover", "peers", "open",  "accept", "close", "ping", "frame",
                                           "ack",      "audio", "input", "lock",   "wake",  "hello"};
  static const QSet<QString> features = {
      "audio-sharing", "remote-viewing", "bidirectional-control", "simultaneous-input", "coordinated-lock-wake"
  };
  if ((!message.source.isEmpty() && !validPeer(message.source)) || !operations.contains(message.operation))
    return false;
  if (message.operation == "discover")
    return message.target.isEmpty() && message.feature.isEmpty() && message.session.isEmpty() && message.body.isEmpty();
  if (!validPeer(message.target))
    return false;
  if (message.operation == "peers")
    return message.feature.isEmpty() && message.session.isEmpty() && message.body.size() == 1 &&
           message.body.value("peers").isArray();
  const auto uuid = QUuid::fromString(message.session);
  return features.contains(message.feature) && !uuid.isNull() &&
         uuid.toString(QUuid::WithoutBraces) == message.session && message.body.size() <= 16;
}

QByteArray encode(const Message &message)
{
  if (!validMessage(message))
    return {};
  const auto bytes = QJsonDocument(
                         QJsonObject{
                             {"v", 1},
                             {"source", message.source},
                             {"target", message.target},
                             {"op", message.operation},
                             {"feature", message.feature},
                             {"session", message.session},
                             {"body", message.body}
                         }
  ).toJson(QJsonDocument::Compact);
  return bytes.size() <= kMaxMessageBytes ? bytes : QByteArray{};
}

std::optional<Message> decode(const QByteArray &bytes)
{
  if (bytes.isEmpty() || bytes.size() > kMaxMessageBytes)
    return std::nullopt;
  QJsonParseError error;
  const auto doc = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject())
    return std::nullopt;
  const auto object = doc.object();
  if (object.size() != 7 || object.value("v") != QJsonValue(1) || !object.value("body").isObject())
    return std::nullopt;
  for (const auto *name : {"source", "target", "op", "feature", "session"})
    if (!object.value(QLatin1String(name)).isString())
      return std::nullopt;
  Message message{object.value("source").toString(),  object.value("target").toString(),
                  object.value("op").toString(),      object.value("feature").toString(),
                  object.value("session").toString(), object.value("body").toObject()};
  return validMessage(message) ? std::optional<Message>(message) : std::nullopt;
}

bool entitled(const Message &message)
{
  if (!Settings::value(Settings::Security::TlsEnabled).toBool())
    return false;
  if (message.operation == "discover" || message.operation == "peers" || message.operation == "close")
    return true;
  return Settings::hasPlanFeature(message.feature);
}

bool SessionRouteBook::observe(
    const Message &message, const QString &sourceConnection, const QString &targetConnection, qint64 now
)
{
  for (auto it = m_routes.begin(); it != m_routes.end();) {
    if (it->expires <= now)
      it = m_routes.erase(it);
    else
      ++it;
  }
  if (message.operation == "hello")
    return true;
  auto found = m_routes.find(message.session);
  if (message.operation == "open") {
    if (found != m_routes.end() || m_routes.size() >= 512)
      return false;
    m_routes.insert(
        message.session,
        {message.source, message.target, message.feature, sourceConnection, targetConnection, now + 60000, false}
    );
    return true;
  }
  if (found == m_routes.end())
    return message.operation == "close";
  const bool forward = found->source == message.source && found->target == message.target &&
                       found->sourceConnection == sourceConnection && found->targetConnection == targetConnection;
  const bool reverse = found->source == message.target && found->target == message.source &&
                       found->sourceConnection == targetConnection && found->targetConnection == sourceConnection;
  if ((!forward && !reverse) || found->feature != message.feature)
    return false;
  if (message.operation == "close") {
    m_routes.erase(found);
    return true;
  }
  if (message.operation == "accept") {
    if (!reverse || found->accepted)
      return false;
    found->accepted = true;
  }
  if (!found->accepted)
    return false;
  found->expires = now + kSessionTimeoutMs;
  return true;
}
void SessionRouteBook::forget(const QString &peer)
{
  for (auto it = m_routes.begin(); it != m_routes.end();) {
    if (it->source == peer || it->target == peer)
      it = m_routes.erase(it);
    else
      ++it;
  }
}

bool InputLeaseBook::observe(const Message &message, qint64 now)
{
  for (auto it = m_leases.begin(); it != m_leases.end();) {
    if (it->expires <= now)
      it = m_leases.erase(it);
    else
      ++it;
  }
  if (message.feature != "bidirectional-control" && message.feature != "simultaneous-input")
    return true;
  auto found = m_leases.find(message.session);
  if (message.operation == "open") {
    if (found == m_leases.end() && m_leases.size() >= 128)
      return false;
    if (found == m_leases.end() && m_leases.size() < 128)
      m_leases.insert(message.session, {message.source, message.target, now + 60000, false});
    return true;
  }
  if (found == m_leases.end())
    return message.operation == "close";
  const bool forward = found->source == message.source && found->target == message.target;
  const bool reverse = found->source == message.target && found->target == message.source;
  if (!forward && !reverse)
    return false;
  if (message.operation == "close") {
    m_leases.erase(found);
    return true;
  }
  if (message.operation == "accept" && reverse)
    found->accepted = true;
  if (found->accepted && (message.operation == "accept" || message.operation == "ping" || message.operation == "input"))
    found->expires = now + kSessionTimeoutMs;
  return message.operation != "accept" || reverse;
}
bool InputLeaseBook::active(qint64 now) const
{
  for (const auto &lease : m_leases)
    if (lease.accepted && lease.expires > now)
      return true;
  return false;
}
void InputLeaseBook::forget(const QString &peer)
{
  for (auto it = m_leases.begin(); it != m_leases.end();) {
    if (it->source == peer || it->target == peer)
      it = m_leases.erase(it);
    else
      ++it;
  }
}
} // namespace deskflow::professional
