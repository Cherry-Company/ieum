/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TailscaleIntegration.h"

#include "NetworkInterfaces.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <algorithm>
#include <iterator>
#include <utility>

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

class QtTailscaleProcess final : public TailscaleIntegration::Process
{
public:
  QtTailscaleProcess() : m_process(new QProcess(QCoreApplication::instance()))
  {
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("TAILSCALE_BE_CLI"), QStringLiteral("1"));
    m_process->setProcessEnvironment(environment);

    m_connections = {
        QObject::connect(
            m_process, &QProcess::started,
            [this] {
              const auto callback = m_callbacks.started;
              if (callback) {
                callback();
              }
            }
        ),
        QObject::connect(
            m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            [this] {
              const auto callback = m_callbacks.finished;
              if (callback) {
                callback();
              }
            }
        ),
        QObject::connect(m_process, &QProcess::errorOccurred, [this] {
          const auto callback = m_callbacks.failed;
          if (callback) {
            callback(m_process->errorString());
          }
        }),
    };
  }

  ~QtTailscaleProcess() override
  {
    m_callbacks = {};
    for (const auto &connection : std::as_const(m_connections)) {
      QObject::disconnect(connection);
    }

    auto *process = std::exchange(m_process, nullptr);
    if (process == nullptr) {
      return;
    }

    if (process->state() == QProcess::NotRunning) {
      process->deleteLater();
      return;
    }

    QObject::connect(
        process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process, &QObject::deleteLater
    );
    QObject::connect(process, &QProcess::errorOccurred, process, [process] {
      if (process->state() == QProcess::NotRunning) {
        process->deleteLater();
      }
    });
    process->kill();
  }

  void setCallbacks(TailscaleIntegration::ProcessCallbacks callbacks) override
  {
    m_callbacks = std::move(callbacks);
  }

  void start(const QString &executable, const QStringList &arguments) override
  {
    m_process->start(executable, arguments, QIODevice::ReadOnly);
  }

  void kill() override
  {
    m_process->kill();
  }

  QByteArray readAllStandardOutput() override
  {
    return m_process->readAllStandardOutput();
  }

  QByteArray readAllStandardError() override
  {
    return m_process->readAllStandardError();
  }

  int exitCode() const override
  {
    return m_process->exitCode();
  }

private:
  QProcess *m_process;
  TailscaleIntegration::ProcessCallbacks m_callbacks;
  QList<QMetaObject::Connection> m_connections;
};

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

TailscaleIntegration::TailscaleIntegration(QObject *parent)
    : TailscaleIntegration(
          [] { return std::make_unique<QtTailscaleProcess>(); }, [] { return executablePath(); }, parent
      )
{
}

TailscaleIntegration::TailscaleIntegration(
    ProcessFactory processFactory, ExecutableResolver executableResolver, QObject *parent
)
    : QObject(parent),
      m_processFactory(std::move(processFactory)),
      m_executableResolver(std::move(executableResolver))
{
  m_timeoutTimer.setSingleShot(true);
  connect(&m_timeoutTimer, &QTimer::timeout, this, [this] {
    finishQuery(
        m_generation, interfaceFallback(TailscaleState::Error, QStringLiteral("Tailscale status timed out")), true
    );
  });
}

TailscaleIntegration::~TailscaleIntegration()
{
  cancel();
}

void TailscaleIntegration::query(int timeoutMs)
{
  if (m_querying) {
    return;
  }

  m_querying = true;
  m_timeoutMs = std::max(1, timeoutMs);
  const auto generation = ++m_generation;
  Q_EMIT queryStarted();

  const auto executable = m_executableResolver();
  if (executable.isEmpty()) {
    QTimer::singleShot(0, this, [this, generation] {
      finishQuery(
          generation, interfaceFallback(TailscaleState::NotInstalled, QStringLiteral("Tailscale command not found"))
      );
    });
    return;
  }

  m_process = m_processFactory();
  if (!m_process) {
    QTimer::singleShot(0, this, [this, generation] {
      finishQuery(generation, interfaceFallback(TailscaleState::Error, QStringLiteral("Could not start Tailscale")));
    });
    return;
  }

  const QPointer<TailscaleIntegration> guard(this);
  m_process->setCallbacks({
      .started =
          [guard, generation] {
            if (guard) {
              guard->processStarted(generation);
            }
          },
      .finished =
          [guard, generation] {
            if (guard) {
              guard->processFinished(generation);
            }
          },
      .failed =
          [guard, generation](const QString &error) {
            if (guard) {
              guard->processFailed(generation, error);
            }
          },
  });
  m_timeoutTimer.start(std::max(1, m_timeoutMs / 2));
  m_process->start(executable, {QStringLiteral("status"), QStringLiteral("--json")});
}

void TailscaleIntegration::cancel()
{
  if (!m_querying && !m_process) {
    return;
  }

  ++m_generation;
  m_querying = false;
  m_timeoutTimer.stop();
  if (m_process) {
    m_process->setCallbacks({});
    m_process->kill();
    m_process.reset();
  }
}

bool TailscaleIntegration::isQuerying() const
{
  return m_querying;
}

void TailscaleIntegration::processStarted(quint64 generation)
{
  if (!isCurrentQuery(generation)) {
    return;
  }
  m_timeoutTimer.start(m_timeoutMs);
}

void TailscaleIntegration::processFinished(quint64 generation)
{
  if (!isCurrentQuery(generation) || !m_process) {
    return;
  }

  const auto standardError = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
  const auto output = m_process->readAllStandardOutput();
  if (output.isEmpty()) {
    const auto error = standardError.isEmpty()
                           ? QStringLiteral("Tailscale status exited with code %1").arg(m_process->exitCode())
                           : standardError;
    finishQuery(generation, interfaceFallback(TailscaleState::Error, error));
    return;
  }

  finishQuery(generation, parseStatus(output, standardError));
}

void TailscaleIntegration::processFailed(quint64 generation, const QString &error)
{
  if (!isCurrentQuery(generation)) {
    return;
  }
  finishQuery(generation, interfaceFallback(TailscaleState::Error, error), true);
}

void TailscaleIntegration::finishQuery(quint64 generation, const TailscaleStatus &status, bool killProcess)
{
  if (!isCurrentQuery(generation)) {
    return;
  }

  m_timeoutTimer.stop();
  m_querying = false;
  if (m_process) {
    m_process->setCallbacks({});
    if (killProcess) {
      m_process->kill();
    }
    m_process.reset();
  }
  Q_EMIT queryFinished(status);
}

bool TailscaleIntegration::isCurrentQuery(quint64 generation) const
{
  return m_querying && generation == m_generation;
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
