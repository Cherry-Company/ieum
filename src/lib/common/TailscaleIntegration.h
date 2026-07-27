/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace deskflow::network {

enum class TailscaleState
{
  NotInstalled,
  Stopped,
  NeedsLogin,
  Running,
  Error
};

struct TailscalePeer
{
  QString id;
  QString name;
  QString dnsName;
  QString os;
  QStringList addresses;
  bool online = false;

  [[nodiscard]] QString preferredAddress() const;
  [[nodiscard]] bool isDesktop() const;
};

struct TailscaleStatus
{
  TailscaleState state = TailscaleState::NotInstalled;
  QString version;
  QString localDnsName;
  QStringList localAddresses;
  QList<TailscalePeer> peers;
  QString error;

  [[nodiscard]] bool isReady() const;
  [[nodiscard]] QString preferredLocalAddress() const;
  [[nodiscard]] QList<TailscalePeer> onlineDesktopPeers() const;
};

class TailscaleIntegration
{
public:
  static TailscaleStatus query(int timeoutMs = 2000);
  static TailscaleStatus parseStatus(const QByteArray &json, const QString &processError = {});
  static QString executablePath();
};

} // namespace deskflow::network
