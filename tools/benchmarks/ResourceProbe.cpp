/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "base/Log.h"
#include "deskflow/ipc/IpcServer.h"
#include "gui/FileTail.h"
#include "gui/widgets/LogWidget.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <sys/resource.h>

namespace {

class ProbeServer final : public deskflow::core::ipc::IpcServer
{
public:
  ProbeServer() : IpcServer(nullptr, "ieum-resource-probe", "probe")
  {
  }

private:
  void processCommand(QLocalSocket *, const QString &, const QStringList &) override
  {
  }
};

qint64 residentKiB()
{
  QFile status("/proc/self/status");
  if (!status.open(QIODevice::ReadOnly)) {
    return -1;
  }
  for (auto line : status.readAll().split('\n')) {
    line = line.simplified();
    if (line.startsWith("VmRSS:")) {
      return line.split(' ').value(1).toLongLong();
    }
  }
  return -1;
}

double cpuMilliseconds()
{
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  return 1000.0 * (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) +
         (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000.0;
}

qint64 peakKiB()
{
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss;
}

} // namespace

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("IeumResourceProbe");
  Log log;
  log.setFilter(LogLevel::Level::Error);
  const auto args = app.arguments();
  if (args.size() < 2) {
    QTextStream(stderr) << "Use ipc-retry, ipc-events, logs-hidden, or tail-idle; optionally count and payload size.\n";
    return 2;
  }
  const auto scenario = args[1];
  const int count = args.value(2, "100000").toInt();
  const int payloadSize = args.value(3, "256").toInt();
  QElapsedTimer elapsed;
  elapsed.start();
  const auto beforeCpu = cpuMilliseconds();
  const auto beforeRss = residentKiB();
  qint64 afterRss = beforeRss;
  int pollTimeouts = 0;

  if (scenario == "ipc-retry" || scenario == "ipc-events") {
    ProbeServer server;
    for (int i = 0; i < count; ++i) {
      auto message = QString::number(i) + QLatin1Char(':') + QString(payloadSize, QLatin1Char('x'));
      server.broadcastCommand(scenario == "ipc-retry" ? "retryIn" : "unrecognisedClient", message);
    }
    afterRss = residentKiB();
  } else if (scenario == "logs-hidden") {
    QWidget hiddenPanel;
    LogWidget widget(&hiddenPanel);
    for (int i = 0; i < count; ++i) {
      widget.appendLine(QString::number(i) + QLatin1Char(':') + QString(payloadSize, QLatin1Char('x')));
    }
    afterRss = residentKiB();
  } else if (scenario == "tail-idle") {
    QTemporaryDir directory;
    QFile file(directory.filePath("core.log"));
    if (!file.open(QIODevice::WriteOnly)) {
      return 3;
    }
    file.write("existing\n");
    file.close();
    deskflow::gui::FileTail tail(file.fileName());
    for (auto *timer : tail.findChildren<QTimer *>()) {
      QObject::connect(timer, &QTimer::timeout, &app, [&pollTimeouts] { ++pollTimeouts; });
    }
    QTimer stop;
    stop.setTimerType(Qt::PreciseTimer);
    stop.setSingleShot(true);
    QObject::connect(&stop, &QTimer::timeout, &app, &QCoreApplication::quit);
    stop.start(count);
    app.exec();
    afterRss = residentKiB();
  } else {
    return 2;
  }

  const QJsonObject result{
      {"scenario", scenario},
      {"count", count},
      {"payloadCharacters", payloadSize},
      {"elapsedMilliseconds", elapsed.elapsed()},
      {"cpuMilliseconds", cpuMilliseconds() - beforeCpu},
      {"rssBeforeKiB", beforeRss},
      {"rssAfterKiB", afterRss},
      {"peakRssKiB", peakKiB()},
      {"pollTimeouts", pollTimeouts}
  };
  QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
  return 0;
}
