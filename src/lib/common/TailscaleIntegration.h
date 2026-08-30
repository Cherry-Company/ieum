/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <memory>

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

class TailscaleIntegration : public QObject
{
  Q_OBJECT

public:
  struct ProcessCallbacks
  {
    std::function<void()> started;
    std::function<void()> finished;
    std::function<void(const QString &)> failed;
  };

  class Process
  {
  public:
    virtual ~Process() = default;
    virtual void setCallbacks(ProcessCallbacks callbacks) = 0;
    virtual void start(const QString &executable, const QStringList &arguments) = 0;
    virtual void kill() = 0;
    virtual QByteArray readAllStandardOutput() = 0;
    virtual QByteArray readAllStandardError() = 0;
    virtual int exitCode() const = 0;
  };

  using ProcessFactory = std::function<std::unique_ptr<Process>()>;
  using ExecutableResolver = std::function<QString()>;

  explicit TailscaleIntegration(QObject *parent = nullptr);
  TailscaleIntegration(ProcessFactory processFactory, ExecutableResolver executableResolver, QObject *parent = nullptr);
  ~TailscaleIntegration() override;

  void query(int timeoutMs = 2000);
  void cancel();
  [[nodiscard]] bool isQuerying() const;

  static TailscaleStatus parseStatus(const QByteArray &json, const QString &processError = {});
  static QString executablePath();

Q_SIGNALS:
  void queryStarted();
  void queryFinished(const deskflow::network::TailscaleStatus &status);

private:
  void processStarted(quint64 generation);
  void processFinished(quint64 generation);
  void processFailed(quint64 generation, const QString &error);
  void finishQuery(quint64 generation, const TailscaleStatus &status, bool killProcess = false);
  [[nodiscard]] bool isCurrentQuery(quint64 generation) const;

  ProcessFactory m_processFactory;
  ExecutableResolver m_executableResolver;
  std::unique_ptr<Process> m_process;
  QTimer m_timeoutTimer;
  quint64 m_generation = 0;
  int m_timeoutMs = 2000;
  bool m_querying = false;
};

} // namespace deskflow::network

Q_DECLARE_METATYPE(deskflow::network::TailscaleStatus)
