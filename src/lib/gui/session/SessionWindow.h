// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#pragma once
#include <QDialog>
#include <QHash>
#include <QPointer>
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
namespace deskflow::gui::session {
class SessionController;
class SessionWindow final : public QDialog
{
  Q_OBJECT
public:
  SessionWindow(SessionController *controller, QWidget *parent = nullptr);

private:
  void refreshPeers();
  void request(const QString &feature);
  void showFrame(const QString &id, const QString &peer, const QImage &image);
  void askConsent(const QString &id, const QString &peer, const QString &feature);
  QString featureName(const QString &feature) const;
  SessionController *m_controller;
  QComboBox *m_peers;
  QLabel *m_status;
  QListWidget *m_sessions;
  QHash<QString, QPointer<QDialog>> m_viewers;
};
} // namespace deskflow::gui::session
