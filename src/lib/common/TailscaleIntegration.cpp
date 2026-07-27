/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TailscaleIntegration.h"

#include "NetworkInterfaces.h"

#include <QAbstractSocket>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <algorithm>
#include <iterator>

namespace deskflow::network {

namespace {

QString choosePreferredAddress(const QStringList &addresses)
{
  for (const auto &address : addresses) {
    if (QHostAddress(address).protocol() == QAbstractSocket::IPv4Protocol) {
      return address;
    }
  }
  return addresses.isEmpty() ? QString() : addresses.first();
}

QString normalizedDnsName(QString name)
{
  while (name.endsWith(QLatin1Char('.'))) {
    name.chop(1);
  }
  return name;
}

QStringList jsonStringList(const QJsonValue &value)
{
  QStringList result;
  for (const auto &entry : value.toArray()) {
    if (const auto text = entry.toString(); !text.isEmpty()) {
      result.append(text);
    }
  }
  return result;
}

TailscaleStatus interfaceFallback(TailscaleState failureState, const QString &error)
{
  TailscaleStatus result;
  result.localAddresses = NetworkInterfaces::tailscaleAddresses();
  result.state = result.localAddresses.isEmpty() ? failureState : TailscaleState::Running;
  result.error = error;
  return result;
}

} // namespace

QString TailscalePeer::preferredAddress() const
{
  return choosePreferredAddress(addresses);
}

bool TailscalePeer::isDesktop() const
{
  static const QStringList desktopSystems = {
      QStringLiteral("windows"), QStringLiteral("macos"),   QStringLiteral("linux"),
      QStringLiteral("freebsd"), QStringLiteral("openbsd"), QStringLiteral("netbsd"),
  };
  return desktopSystems.contains(os.toLower());
}

bool TailscaleStatus::isReady() const
{
  return state == TailscaleState::Running && !preferredLocalAddress().isEmpty();
}

QString TailscaleStatus::preferredLocalAddress() const
{
  return choosePreferredAddress(localAddresses);
}

QList<TailscalePeer> TailscaleStatus::onlineDesktopPeers() const
{
  QList<TailscalePeer> result;
  std::ranges::copy_if(peers, std::back_inserter(result), [](const auto &peer) {
    return peer.online && peer.isDesktop() && !peer.preferredAddress().isEmpty();
  });
  return result;
}

QString TailscaleIntegration::executablePath()
{
  if (const auto executable = QStandardPaths::findExecutable(QStringLiteral("tailscale")); !executable.isEmpty()) {
    return executable;
  }

  QStringList candidates;
#if defined(Q_OS_WIN)
  for (const auto &root : {qEnvironmentVariable("ProgramW6432"), qEnvironmentVariable("ProgramFiles")}) {
    if (!root.isEmpty()) {
      candidates.append(QDir(root).filePath(QStringLiteral("Tailscale/tailscale.exe")));
    }
  }
#elif defined(Q_OS_MACOS)
  candidates = {
      QStringLiteral("/Applications/Tailscale.app/Contents/MacOS/Tailscale"),
      QStringLiteral("/Applications/Tailscale.app/Contents/MacOS/tailscale"),
      QStringLiteral("/usr/local/bin/tailscale"),
      QStringLiteral("/opt/homebrew/bin/tailscale"),
  };
#else
  candidates = {QStringLiteral("/usr/bin/tailscale"), QStringLiteral("/usr/local/bin/tailscale")};
#endif

  for (const auto &candidate : std::as_const(candidates)) {
    if (QFileInfo(candidate).isExecutable()) {
      return candidate;
    }
  }
  return {};
}

TailscaleStatus TailscaleIntegration::query(int timeoutMs)
{
  const auto executable = executablePath();
  if (executable.isEmpty()) {
    return interfaceFallback(TailscaleState::NotInstalled, QStringLiteral("Tailscale command not found"));
  }

  QProcess process;
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("TAILSCALE_BE_CLI"), QStringLiteral("1"));
  process.setProcessEnvironment(environment);
  process.start(executable, {QStringLiteral("status"), QStringLiteral("--json")}, QIODevice::ReadOnly);
  if (!process.waitForStarted(std::max(250, timeoutMs / 2))) {
    return interfaceFallback(TailscaleState::Error, process.errorString());
  }

  if (!process.waitForFinished(std::max(250, timeoutMs))) {
    process.kill();
    process.waitForFinished();
    return interfaceFallback(TailscaleState::Error, QStringLiteral("Tailscale status timed out"));
  }

  const auto standardError = QString::fromUtf8(process.readAllStandardError()).trimmed();
  const auto output = process.readAllStandardOutput();
  if (output.isEmpty()) {
    const auto error = standardError.isEmpty()
                           ? QStringLiteral("Tailscale status exited with code %1").arg(process.exitCode())
                           : standardError;
    return interfaceFallback(TailscaleState::Error, error);
  }

  return parseStatus(output, standardError);
}

TailscaleStatus TailscaleIntegration::parseStatus(const QByteArray &json, const QString &processError)
{
  TailscaleStatus result;
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    result.state = TailscaleState::Error;
    result.error = parseError.errorString();
    return result;
  }

  const auto root = document.object();
  result.version = root.value(QStringLiteral("Version")).toString();
  result.localAddresses = jsonStringList(root.value(QStringLiteral("TailscaleIPs")));

  const auto self = root.value(QStringLiteral("Self")).toObject();
  if (result.localAddresses.isEmpty()) {
    result.localAddresses = jsonStringList(self.value(QStringLiteral("TailscaleIPs")));
  }
  result.localDnsName = normalizedDnsName(self.value(QStringLiteral("DNSName")).toString());

  const auto backendState = root.value(QStringLiteral("BackendState")).toString();
  if (backendState.compare(QStringLiteral("Running"), Qt::CaseInsensitive) == 0) {
    result.state = TailscaleState::Running;
  } else if (backendState.contains(QStringLiteral("login"), Qt::CaseInsensitive) ||
             backendState.contains(QStringLiteral("auth"), Qt::CaseInsensitive)) {
    result.state = TailscaleState::NeedsLogin;
  } else if (backendState.compare(QStringLiteral("Stopped"), Qt::CaseInsensitive) == 0 ||
             backendState.compare(QStringLiteral("NoState"), Qt::CaseInsensitive) == 0) {
    result.state = TailscaleState::Stopped;
  } else {
    result.state = TailscaleState::Error;
  }

  const auto peers = root.value(QStringLiteral("Peer")).toObject();
  for (auto it = peers.constBegin(); it != peers.constEnd(); ++it) {
    const auto object = it.value().toObject();
    TailscalePeer peer;
    peer.id = object.value(QStringLiteral("ID")).toString();
    peer.dnsName = normalizedDnsName(object.value(QStringLiteral("DNSName")).toString());
    peer.name = peer.dnsName.section(QLatin1Char('.'), 0, 0);
    if (peer.name.isEmpty()) {
      peer.name = object.value(QStringLiteral("HostName")).toString();
    }
    peer.os = object.value(QStringLiteral("OS")).toString();
    peer.addresses = jsonStringList(object.value(QStringLiteral("TailscaleIPs")));
    peer.online = object.value(QStringLiteral("Online")).toBool();
    result.peers.append(peer);
  }

  std::ranges::sort(result.peers, [](const auto &left, const auto &right) {
    if (left.online != right.online) {
      return left.online;
    }
    return left.name.compare(right.name, Qt::CaseInsensitive) < 0;
  });

  result.error = processError;
  if (result.state == TailscaleState::Running && result.localAddresses.isEmpty()) {
    result.state = TailscaleState::Error;
    result.error = QStringLiteral("Tailscale has no local address");
  }
  return result;
}

} // namespace deskflow::network
