/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2025 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FileTail.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTextStream>
#include <QTimer>

namespace deskflow::gui {

namespace {

constexpr auto kFallbackPollIntervalMs = 250;
constexpr auto kFileIdentityPrefixLength = 64;

qint64 birthTimeMsecs(const QFile &file)
{
  const auto birthTime = QFileInfo(file).birthTime();
  return birthTime.isValid() ? birthTime.toMSecsSinceEpoch() : -1;
}

} // namespace

FileTail::FileTail(const QString &filePath, QObject *parent)
    : QObject(parent),
      m_watcher(new QFileSystemWatcher(this)),
      m_pollTimer(new QTimer(this))
{
  m_pollTimer->setInterval(kFallbackPollIntervalMs);
  connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &FileTail::handleFileChanged);
  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &FileTail::handleDirectoryChanged);
  connect(m_pollTimer, &QTimer::timeout, this, &FileTail::pollWatchedFile);
  setWatchedFile(filePath);
}

void FileTail::setWatchedFile(const QString &filePath)
{
  if (filePath.isEmpty())
    return;

  const auto absoluteFilePath = QFileInfo(filePath).absoluteFilePath();
  if (m_filePath == absoluteFilePath) {
    if (!m_watcher->files().contains(m_filePath))
      watchCurrentFile(false);
    else
      m_pollTimer->start();
    return;
  }

  m_pollTimer->stop();
  if (!m_filePath.isEmpty() && m_watcher->files().contains(m_filePath))
    m_watcher->removePath(m_filePath);
  if (!m_directoryPath.isEmpty() && m_watcher->directories().contains(m_directoryPath))
    m_watcher->removePath(m_directoryPath);

  m_filePath = absoluteFilePath;
  m_directoryPath = QFileInfo(m_filePath).absolutePath();
  m_lastPos = 0;
  clearFileIdentity();

  if (!m_watcher->addPath(m_directoryPath))
    qWarning() << "failed to watch directory for tail:" << m_directoryPath;

  watchCurrentFile(true);
}

bool FileTail::watchCurrentFile(bool startAtEnd)
{
  QFile file(m_filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    if (!m_pollTimer->isActive())
      qDebug() << "waiting for file to tail:" << m_filePath;
    m_pollTimer->start();
    return false;
  }

  if (!m_watcher->files().contains(m_filePath) && !m_watcher->addPath(m_filePath)) {
    if (!m_pollTimer->isActive())
      qWarning() << "failed to watch file for tail:" << m_filePath;
  }

  m_pollTimer->start();
  qDebug() << "starting file tail:" << m_filePath;
  if (startAtEnd) {
    m_lastPos = file.size();
    updateFileIdentity(file);
  } else {
    file.close();
    readNewLines();
  }

  return true;
}

void FileTail::readNewLines()
{
  QFile file(m_filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    if (!QFileInfo::exists(m_filePath)) {
      m_lastPos = 0;
      clearFileIdentity();
    }
    return;
  }

  if (file.size() < m_lastPos || hasFileIdentityChanged(file))
    m_lastPos = 0;

  if (!file.seek(m_lastPos)) {
    qWarning() << "failed to seek file for tail:" << m_filePath;
    return;
  }

  QTextStream stream(&file);
  while (!stream.atEnd()) {
    Q_EMIT newLine(stream.readLine());
  }
  m_lastPos = file.pos();
  updateFileIdentity(file);
}

bool FileTail::hasFileIdentityChanged(QFile &file) const
{
  const auto currentBirthTime = birthTimeMsecs(file);
  const bool birthTimeChanged = m_fileBirthTime >= 0 && currentBirthTime >= 0 && m_fileBirthTime != currentBirthTime;

  const auto originalPosition = file.pos();
  if (!file.seek(0))
    return birthTimeChanged;

  const auto currentPrefix = file.read(kFileIdentityPrefixLength);
  file.seek(originalPosition);
  const bool prefixChanged = !m_filePrefix.isEmpty() && currentPrefix.left(m_filePrefix.size()) != m_filePrefix;
  return birthTimeChanged || prefixChanged;
}

void FileTail::updateFileIdentity(QFile &file)
{
  m_fileBirthTime = birthTimeMsecs(file);

  const auto originalPosition = file.pos();
  if (file.seek(0))
    m_filePrefix = file.read(kFileIdentityPrefixLength);
  file.seek(originalPosition);
}

void FileTail::clearFileIdentity()
{
  m_fileBirthTime = -1;
  m_filePrefix.clear();
}

void FileTail::handleFileChanged(const QString &filePath)
{
  if (filePath != m_filePath)
    return;

  if (!m_watcher->files().contains(m_filePath)) {
    m_lastPos = 0;
    clearFileIdentity();
    watchCurrentFile(false);
    return;
  }

  readNewLines();
}

void FileTail::handleDirectoryChanged(const QString &directoryPath)
{
  if (directoryPath != m_directoryPath || !QFileInfo::exists(m_filePath) || m_watcher->files().contains(m_filePath)) {
    return;
  }

  m_lastPos = 0;
  clearFileIdentity();
  watchCurrentFile(false);
}

void FileTail::pollWatchedFile()
{
  if (m_filePath.isEmpty()) {
    m_pollTimer->stop();
    return;
  }

  if (!QFileInfo::exists(m_filePath)) {
    m_lastPos = 0;
    clearFileIdentity();
  } else if (!m_watcher->files().contains(m_filePath)) {
    watchCurrentFile(false);
  } else {
    readNewLines();
  }
}

} // namespace deskflow::gui
