/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "VersionChecker.h"

#include "common/Settings.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"

#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSysInfo>
#include <climits>

VersionChecker::VersionChecker(QObject *parent) : QObject(parent), m_network{new QNetworkAccessManager(this)}
{
  connect(m_network, &QNetworkAccessManager::finished, this, &VersionChecker::replyFinished, Qt::UniqueConnection);
}

void VersionChecker::checkLatest() const
{
  const QString url = Settings::value(Settings::Gui::UpdateCheckUrl).toString();
  qDebug("checking for updates at: %s", qPrintable(url));
  auto request = QNetworkRequest(url);
  auto userAgent = QString("%1 %2 on %3").arg(kAppName, kVersion, QSysInfo::prettyProductName());
  request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
  request.setRawHeader("X-Ieum-Version", kVersion);
  request.setRawHeader("X-Ieum-Language", qPrintable(QLocale::system().name()));
  m_network->get(request);
}

QString VersionChecker::packageFileName(
    const QString &version, const QString &platform, const QString &architecture, bool korean
)
{
  static const QRegularExpression versionPattern(
      QStringLiteral(R"(^\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)\.\d+)?$)"), QRegularExpression::CaseInsensitiveOption
  );
  if (!versionPattern.match(version).hasMatch()) {
    return {};
  }

  if (platform == QStringLiteral("windows")) {
    const auto assetArchitecture = architecture.contains(QStringLiteral("arm"), Qt::CaseInsensitive)
                                       ? QStringLiteral("arm64")
                                       : QStringLiteral("x64");
    return QStringLiteral("Ieum-%1-win-%2%3.msi")
        .arg(version, assetArchitecture, korean ? QStringLiteral("-ko-KR") : QString{});
  }
  if (platform == QStringLiteral("macos")) {
    const auto assetArchitecture = architecture.contains(QStringLiteral("arm"), Qt::CaseInsensitive)
                                       ? QStringLiteral("arm64")
                                       : QStringLiteral("x86_64");
    return QStringLiteral("Ieum-%1-macos-%2.dmg").arg(version, assetArchitecture);
  }
  return {};
}

QUrl VersionChecker::releasePageUrl(const QString &version)
{
  const auto normalized = version.startsWith(QLatin1Char('v')) ? version.mid(1) : version;
  if (packageFileName(normalized, QStringLiteral("windows"), QStringLiteral("x64"), false).isEmpty()) {
    return QUrl(kUrlDownload);
  }
  return QUrl(QStringLiteral("%1/tag/v%2").arg(kUrlDownload, normalized));
}

QUrl VersionChecker::packageDownloadUrl(const QString &version)
{
  const auto normalized = version.startsWith(QLatin1Char('v')) ? version.mid(1) : version;
  QString platform;
#if defined(Q_OS_WIN)
  platform = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
  platform = QStringLiteral("macos");
#else
  return {};
#endif

  const auto fileName = packageFileName(
      normalized, platform, QSysInfo::currentCpuArchitecture(), QLocale::system().language() == QLocale::Korean
  );
  if (fileName.isEmpty()) {
    return {};
  }
  return QUrl(QStringLiteral("%1/download/v%2/%3").arg(kUrlDownload, normalized, fileName));
}

void VersionChecker::replyFinished(QNetworkReply *reply)
{
  const auto httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (reply->error() != QNetworkReply::NoError) {
    qWarning("version check server error: %s", qPrintable(reply->errorString()));
    qWarning("version check server response: %s", qPrintable(QString(reply->readAll())));
    qWarning("error checking for updates, http status: %d", httpStatus);
    reply->deleteLater();
    return;
  }

  qDebug("version check server success, http status: %d", httpStatus);

  const auto newestVersion = QString(reply->readAll()).trimmed();
  reply->deleteLater();
  qDebug("version check response: %s", qPrintable(newestVersion));

  if (newestVersion.isEmpty()) {
    qWarning() << "version check is response is empty";
    return;
  }
  if (packageFileName(newestVersion, QStringLiteral("windows"), QStringLiteral("x64"), false).isEmpty()) {
    qWarning("version check returned an invalid version: %s", qPrintable(newestVersion));
    return;
  }

  if (compareVersions(kVersion, newestVersion) > 0) {
    qWarning().noquote() //
        << QStringLiteral("current version %1 out of date, update available: %2").arg(kVersion, newestVersion);
    Q_EMIT updateFound(newestVersion);
  } else {
    qDebug().noquote() << QStringLiteral("current version %1 is upto date").arg(kVersion);
  }
}

int VersionChecker::getStageVersion(QString stage)
{
  const char *stableName = "stable";
  const char *rcName = "rc";
  const char *betaName = "beta";
  const char *alphaName = "alpha";

  // use max int for stable so it's always the highest value.
  const int stableValue = INT_MAX;
  const int rcValue = 3000;
  const int betaValue = 2000;
  const int alphaValue = 1000;
  const int otherValue = 0;

  if (stage.isEmpty() || stage == stableName) {
    return stableValue;
  } else if (stage.startsWith(rcName, Qt::CaseInsensitive) || stage.startsWith(betaName, Qt::CaseInsensitive) ||
             stage.startsWith(alphaName, Qt::CaseInsensitive)) {
    static QRegularExpression re("(\\d+)$", QRegularExpression::CaseInsensitiveOption);
    auto match = re.match(stage);
    const int sequence = match.hasMatch() ? match.captured(1).toInt() : 0;
    if (stage.startsWith(rcName, Qt::CaseInsensitive))
      return rcValue + sequence;
    if (stage.startsWith(betaName, Qt::CaseInsensitive))
      return betaValue + sequence;
    return alphaValue + sequence;
  }

  return otherValue;
}

int VersionChecker::compareVersions(const QString &left, const QString &right)
{
  if (left.compare(right) == 0)
    return 0; // versions are same.

  QStringList leftParts = left.split("-");
  QStringList rightParts = right.split("-");

  QString leftNumber = leftParts.at(0);
  QString rightNumber = rightParts.at(0);

  QStringList leftNumberParts = leftNumber.split(".");
  QStringList rightNumberParts = rightNumber.split(".");

  auto leftStagePart = leftParts.size() > 1 ? leftParts.at(1) : "";
  auto rightStagePart = rightParts.size() > 1 ? rightParts.at(1) : "";

  const int leftMajor = leftNumberParts.at(0).toInt();
  const int leftMinor = leftNumberParts.at(1).toInt();
  const int leftPatch = leftNumberParts.at(2).toInt();
  const int leftStage = getStageVersion(leftStagePart);

  const int rightMajor = rightNumberParts.at(0).toInt();
  const int rightMinor = rightNumberParts.at(1).toInt();
  const int rightPatch = rightNumberParts.at(2).toInt();
  const int rightStage = getStageVersion(rightStagePart);

  const bool rightWins =
      (rightMajor > leftMajor) || ((rightMajor >= leftMajor) && (rightMinor > leftMinor)) ||
      ((rightMajor >= leftMajor) && (rightMinor >= leftMinor) && (rightPatch > leftPatch)) ||
      ((rightMajor >= leftMajor) && (rightMinor >= leftMinor) && (rightPatch >= leftPatch) && (rightStage > leftStage));

  return rightWins ? 1 : -1;
}
