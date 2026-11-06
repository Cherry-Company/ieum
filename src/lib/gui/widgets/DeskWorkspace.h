/*
 * Ieum -- keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/config/ServerConfig.h"

#include <QWidget>

class QBoxLayout;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QScrollArea;
class QToolButton;
class ScreenSetupModel;
class ScreenSetupView;

class DeskWorkspace final : public QWidget
{
  Q_OBJECT

public:
  explicit DeskWorkspace(ServerConfig &config, QWidget *parent = nullptr);
  void refresh(bool host, const QString &localName, const QStringList &connectedComputers);
  void setStatus(const QString &title, const QString &detail, bool connected, bool sharing);
  void retranslate();

Q_SIGNALS:
  void configurationCommitted();
  void connectionSettingsRequested();
  void advancedSettingsRequested();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  void reload();
  void rebuildOverview();
  void selectComputer(int index);
  void updateInspector();
  void setEditing(bool editing);
  void saveLayout();
  void cancelEditing();
  void addComputer();
  void moveComputer(const QPoint &direction);
  void editComputer();
  void removeComputer();
  bool isConnected(const Screen &screen) const;
  bool canEdit() const;
  QString positionText(int index) const;

  ServerConfig &m_config;
  ScreenList m_screens;
  ScreenList m_baseline;
  QString m_localName;
  QStringList m_connected;
  bool m_host = false;
  bool m_external = false;
  bool m_editing = false;
  bool m_sharing = false;
  int m_selected = -1;
  int m_columns = 0;
  int m_rows = 0;
  ScreenSetupModel *m_model = nullptr;
  ScreenSetupView *m_editor;
  QLabel *m_title;
  QLabel *m_status;
  QLabel *m_detail;
  QLabel *m_hint;
  QLabel *m_feedback;
  QLabel *m_deviceName;
  QLabel *m_deviceStatus;
  QLabel *m_recoveryDetail;
  QLabel *m_position;
  QWidget *m_overview;
  QScrollArea *m_overviewScroll;
  QWidget *m_inspector;
  QScrollArea *m_inspectorScroll;
  QWidget *m_addForm;
  QWidget *m_directions;
  QBoxLayout *m_body;
  QGridLayout *m_devices;
  QStackedWidget *m_views;
  QPushButton *m_edit;
  QPushButton *m_add;
  QPushButton *m_cancel;
  QPushButton *m_deviceSettings;
  QPushButton *m_connectionHelp;
  QPushButton *m_remove;
  QPushButton *m_advanced;
  QToolButton *m_closeInspector;
  QLineEdit *m_newName;
  QComboBox *m_newSide;
  QPushButton *m_confirmAdd;
};
