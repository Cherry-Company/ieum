/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2025 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QFile;
class QFileSystemWatcher;
class QTimer;

namespace deskflow::gui {

class FileTail : public QObject
{
  Q_OBJECT

public:
  explicit FileTail(const QString &filePath, QObject *parent = nullptr);
  void setWatchedFile(const QString &filePath);

Q_SIGNALS:
  void newLine(const QString &line);

private Q_SLOTS:
  void handleFileChanged(const QString &filePath);
  void handleDirectoryChanged(const QString &directoryPath);
  void pollWatchedFile();

private:
  bool watchCurrentFile(bool startAtEnd);
  void readNewLines();
  bool hasFileIdentityChanged(QFile &file) const;
  void updateFileIdentity(QFile &file);
  void clearFileIdentity();

  QString m_filePath;
  QString m_directoryPath;
  QFileSystemWatcher *m_watcher = nullptr;
  QTimer *m_pollTimer = nullptr;
  QByteArray m_filePrefix;
  qint64 m_fileBirthTime = -1;
  qint64 m_lastPos = 0;
};

} // namespace deskflow::gui
