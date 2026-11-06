// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace deskflow::professional {
inline constexpr qsizetype kMaxMessageBytes = 24 * 1024;
inline constexpr qsizetype kChunkBytes = 12 * 1024;
inline constexpr qsizetype kMaxFrameBytes = 384 * 1024;
inline constexpr int kProtocolMinor = 15;
inline constexpr int kSessionTimeoutMs = 6000;

struct Message
{
  QString source;
  QString target;
  QString operation;
  QString feature;
  QString session;
  QJsonObject body;
};
bool validPeer(const QString &name);
bool validMessage(const Message &message);
QByteArray encode(const Message &message);
std::optional<Message> decode(const QByteArray &bytes);
bool entitled(const Message &message);

class SessionRouteBook
{
public:
  bool observe(const Message &message, const QString &sourceConnection, const QString &targetConnection, qint64 now);
  void forget(const QString &peer);

private:
  struct Route
  {
    QString source, target, feature, sourceConnection, targetConnection;
    qint64 expires = 0;
    bool accepted = false;
  };
  QHash<QString, Route> m_routes;
};

// The hub also arbitrates sessions between two secondary computers. Only a
// matching receiver's acceptance pauses ordinary KVM forwarding.
class InputLeaseBook
{
public:
  bool observe(const Message &message, qint64 now);
  bool active(qint64 now) const;
  void forget(const QString &peer);

private:
  struct Lease
  {
    QString source, target;
    qint64 expires = 0;
    bool accepted = false;
  };
  QHash<QString, Lease> m_leases;
};
} // namespace deskflow::professional
