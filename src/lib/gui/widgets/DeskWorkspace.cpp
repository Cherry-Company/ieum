/*
 * Ieum -- keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DeskWorkspace.h"
#include "common/Settings.h"

#include "gui/ScreenSetupModel.h"
#include "gui/dialogs/ScreenSettingsDialog.h"
#include "gui/validators/ComputerNameValidator.h"
#include "gui/widgets/ScreenSetupView.h"

#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QImageReader>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPixmapCache>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QIcon computerIcon(QWidget *widget)
{
  QPixmap image;
  const auto key = QStringLiteral("ieum-desk-computer");
  if (!QPixmapCache::find(key, &image)) {
    QImageReader reader(QStringLiteral(":/image/desk-computer.png"));
    reader.setScaledSize(reader.size().scaled(480, 320, Qt::KeepAspectRatio));
    image = QPixmap::fromImage(reader.read());
    if (!image.isNull())
      QPixmapCache::insert(key, image);
  }
  return image.isNull() ? widget->style()->standardIcon(QStyle::SP_ComputerIcon) : QIcon(image);
}

void plainLabel(QLabel *label)
{
  label->setTextFormat(Qt::PlainText);
  label->setWordWrap(true);
  label->setMinimumWidth(0);
}
} // namespace

DeskWorkspace::DeskWorkspace(ServerConfig &config, QWidget *parent)
    : QWidget(parent),
      m_config(config),
      m_screens(config.screens()),
      m_baseline(config.screens())
{
  setObjectName(QStringLiteral("deskWorkspace"));
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(8, 12, 8, 8);
  root->setSpacing(12);
  auto *heading = new QHBoxLayout;
  m_title = new QLabel(this);
  m_title->setObjectName(QStringLiteral("deskTitle"));
  heading->addWidget(m_title, 1);
  m_cancel = new QPushButton(this);
  m_cancel->setObjectName(QStringLiteral("cancelDeskLayout"));
  heading->addWidget(m_cancel);
  m_edit = new QPushButton(this);
  m_edit->setObjectName(QStringLiteral("editDeskLayout"));
  m_edit->setProperty("primaryAction", true);
  heading->addWidget(m_edit);
  root->addLayout(heading);
  m_status = new QLabel(this);
  m_status->setObjectName(QStringLiteral("deskStatus"));
  plainLabel(m_status);
  root->addWidget(m_status);
  m_detail = new QLabel(this);
  m_detail->setObjectName(QStringLiteral("deskDetail"));
  m_detail->setProperty("secondary", true);
  plainLabel(m_detail);
  root->addWidget(m_detail);

  m_body = new QBoxLayout(QBoxLayout::LeftToRight);
  m_body->setSpacing(20);
  m_views = new QStackedWidget(this);
  m_views->setMinimumHeight(200);
  m_overview = new QWidget(this);
  m_devices = new QGridLayout(m_overview);
  m_devices->setContentsMargins(0, 12, 0, 12);
  m_devices->setSpacing(12);
  m_overviewScroll = new QScrollArea(this);
  m_overviewScroll->setObjectName(QStringLiteral("deskOverviewScroll"));
  m_overviewScroll->setWidgetResizable(true);
  m_overviewScroll->setFrameShape(QFrame::NoFrame);
  m_overviewScroll->viewport()->setAutoFillBackground(false);
  m_overviewScroll->setWidget(m_overview);
  m_overview->setAutoFillBackground(false);
  m_views->addWidget(m_overviewScroll);
  m_editor = new ScreenSetupView(this);
  m_editor->setObjectName(QStringLiteral("deskLayoutEditor"));
  m_editor->setMinimumSize(0, 240);
  m_views->addWidget(m_editor);
  m_body->addWidget(m_views, 1);

  m_inspector = new QWidget(this);
  m_inspector->setObjectName(QStringLiteral("deviceInspector"));
  m_inspector->setMinimumWidth(0);
  auto *inspector = new QVBoxLayout(m_inspector);
  inspector->setContentsMargins(18, 8, 0, 8);
  inspector->setSpacing(12);
  auto *deviceHeading = new QHBoxLayout;
  m_deviceName = new QLabel(this);
  m_deviceName->setObjectName(QStringLiteral("deskDeviceName"));
  plainLabel(m_deviceName);
  deviceHeading->addWidget(m_deviceName, 1);
  m_closeInspector = new QToolButton(this);
  m_closeInspector->setObjectName(QStringLiteral("closeDeviceInspector"));
  m_closeInspector->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
  m_closeInspector->setMinimumSize(36, 36);
  m_closeInspector->setAutoRaise(true);
  deviceHeading->addWidget(m_closeInspector);
  inspector->addLayout(deviceHeading);
  m_deviceStatus = new QLabel(this);
  m_deviceStatus->setObjectName(QStringLiteral("deskDeviceStatus"));
  plainLabel(m_deviceStatus);
  inspector->addWidget(m_deviceStatus);
  m_recoveryDetail = new QLabel(this);
  m_recoveryDetail->setObjectName(QStringLiteral("deskDeviceRecoveryDetail"));
  plainLabel(m_recoveryDetail);
  inspector->addWidget(m_recoveryDetail);
  m_connectionHelp = new QPushButton(this);
  m_connectionHelp->setObjectName(QStringLiteral("deskDeviceConnectionHelp"));
  inspector->addWidget(m_connectionHelp);
  m_position = new QLabel(this);
  m_position->setObjectName(QStringLiteral("deskDevicePosition"));
  plainLabel(m_position);
  inspector->addWidget(m_position);
  m_directions = new QWidget(this);
  auto *directions = new QGridLayout(m_directions);
  directions->setContentsMargins(0, 8, 0, 8);
  const QList<QPoint> deltas{{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
  const QList<Qt::ArrowType> arrows{Qt::UpArrow, Qt::LeftArrow, Qt::RightArrow, Qt::DownArrow};
  const QList<QPoint> positions{{1, 0}, {0, 1}, {2, 1}, {1, 2}};
  for (int i = 0; i < deltas.size(); ++i) {
    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("deskMove%1").arg(i));
    button->setArrowType(arrows[i]);
    button->setMinimumSize(38, 38);
    directions->addWidget(button, positions[i].y(), positions[i].x(), Qt::AlignCenter);
    connect(button, &QToolButton::clicked, this, [this, delta = deltas[i]] { moveComputer(delta); });
  }
  inspector->addWidget(m_directions);
  m_deviceSettings = new QPushButton(this);
  m_deviceSettings->setObjectName(QStringLiteral("deskDeviceSettings"));
  inspector->addWidget(m_deviceSettings);
  m_remove = new QPushButton(this);
  m_remove->setObjectName(QStringLiteral("deskRemoveComputer"));
  inspector->addWidget(m_remove);
  inspector->addStretch();
  m_inspectorScroll = new QScrollArea(this);
  m_inspectorScroll->setObjectName(QStringLiteral("deskInspectorScroll"));
  m_inspectorScroll->setWidgetResizable(true);
  m_inspectorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_inspectorScroll->setFrameShape(QFrame::NoFrame);
  m_inspectorScroll->viewport()->setAutoFillBackground(false);
  m_inspectorScroll->setWidget(m_inspector);
  m_inspector->setAutoFillBackground(false);
  m_inspectorScroll->setMinimumSize(0, 0);
  m_body->addWidget(m_inspectorScroll);
  root->addLayout(m_body, 1);

  m_addForm = new QWidget(this);
  auto *form = new QGridLayout(m_addForm);
  form->setContentsMargins(0, 0, 0, 0);
  m_newName = new QLineEdit(this);
  m_newName->setObjectName(QStringLiteral("deskNewComputerName"));
  m_newName->setMaxLength(255);
  form->addWidget(m_newName, 0, 0, 1, 2);
  m_newSide = new QComboBox(this);
  m_newSide->setObjectName(QStringLiteral("deskNewComputerSide"));
  for (const QPoint &delta : {QPoint(1, 0), QPoint(-1, 0), QPoint(0, -1), QPoint(0, 1)})
    m_newSide->addItem(QString(), delta);
  form->addWidget(m_newSide, 1, 0);
  m_confirmAdd = new QPushButton(this);
  m_confirmAdd->setObjectName(QStringLiteral("deskConfirmAddComputer"));
  form->addWidget(m_confirmAdd, 1, 1);
  root->addWidget(m_addForm);
  m_feedback = new QLabel(this);
  m_feedback->setObjectName(QStringLiteral("deskFeedback"));
  plainLabel(m_feedback);
  root->addWidget(m_feedback);
  auto *footer = new QHBoxLayout;
  m_hint = new QLabel(this);
  m_hint->setProperty("secondary", true);
  plainLabel(m_hint);
  footer->addWidget(m_hint, 1);
  m_advanced = new QPushButton(this);
  m_advanced->setObjectName(QStringLiteral("deskAdvancedSettings"));
  footer->addWidget(m_advanced);
  m_add = new QPushButton(this);
  m_add->setObjectName(QStringLiteral("deskAddComputer"));
  footer->addWidget(m_add);
  root->addLayout(footer);

  connect(m_edit, &QPushButton::clicked, this, [this] {
    if (!canEdit()) {
      Q_EMIT advancedSettingsRequested();
    } else if (m_editing) {
      saveLayout();
    } else {
      setEditing(true);
    }
  });
  connect(m_cancel, &QPushButton::clicked, this, &DeskWorkspace::cancelEditing);
  connect(m_closeInspector, &QToolButton::clicked, this, [this] { selectComputer(-1); });
  connect(m_add, &QPushButton::clicked, this, [this] {
    if (!canEdit()) {
      Q_EMIT connectionSettingsRequested();
      return;
    }
    setEditing(true);
    m_addForm->show();
    m_newName->setFocus();
  });
  connect(m_confirmAdd, &QPushButton::clicked, this, &DeskWorkspace::addComputer);
  connect(m_newName, &QLineEdit::returnPressed, this, &DeskWorkspace::addComputer);
  connect(m_deviceSettings, &QPushButton::clicked, this, &DeskWorkspace::editComputer);
  connect(m_connectionHelp, &QPushButton::clicked, this, &DeskWorkspace::connectionSettingsRequested);
  connect(m_remove, &QPushButton::clicked, this, &DeskWorkspace::removeComputer);
  connect(m_advanced, &QPushButton::clicked, this, &DeskWorkspace::advancedSettingsRequested);
  m_inspector->hide();
  m_inspectorScroll->hide();
  m_addForm->hide();
  m_feedback->hide();
  m_cancel->hide();
  reload();
  retranslate();
}

bool DeskWorkspace::canEdit() const
{
  return m_host && !m_config.useExternalConfig();
}

void DeskWorkspace::reload()
{
  m_screens = m_config.screens();
  m_baseline = m_screens;
  m_columns = Settings::value(Settings::Server::GridWidth).toInt();
  m_rows = Settings::value(Settings::Server::GridHeight).toInt();
  // A fresh internal configuration has empty cells until its first peer is
  // added. Show this computer immediately, but keep it in the draft until Save.
  bool hasLocal = false;
  for (const auto &screen : m_screens)
    hasLocal = hasLocal || screen.isServer();
  if (!hasLocal) {
    const auto name = m_localName.isEmpty() ? m_config.getServerName() : m_localName;
    int local = -1;
    for (int i = 0; i < m_screens.size(); ++i) {
      if (m_screens[i].name() == name)
        local = i;
    }
    const int center = (m_rows / 2) * m_columns + m_columns / 2;
    if (local < 0 && m_screens.value(center).isNull())
      local = center;
    if (local < 0) {
      for (int i = 0; i < m_screens.size(); ++i) {
        if (m_screens[i].isNull()) {
          local = i;
          break;
        }
      }
    }
    if (local >= 0 && local < m_screens.size()) {
      m_screens[local].setName(name);
      m_screens[local].markAsServer();
    }
  }
  auto *old = m_model;
  auto *oldSelection = m_editor->selectionModel();
  m_model = new ScreenSetupModel(m_screens, m_columns, m_rows);
  m_model->setParent(this);
  m_editor->setModel(m_model);
  delete oldSelection;
  delete old;
  connect(m_editor->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) {
    selectComputer(index.isValid() ? index.row() * m_columns + index.column() : -1);
  });
  connect(m_model, &ScreenSetupModel::screensChanged, this, [this] {
    updateInspector();
    m_editor->viewport()->update();
  });
  selectComputer(-1);
  rebuildOverview();
}

void DeskWorkspace::refresh(bool host, const QString &localName, const QStringList &connectedComputers)
{
  const bool roleChanged = m_host != host;
  const bool layoutChanged = !m_editing && !(m_baseline == m_config.screens());
  const bool nameChanged = m_localName != localName;
  const bool externalChanged = m_external != m_config.useExternalConfig();
  const bool changed =
      roleChanged || externalChanged || layoutChanged || m_localName != localName || m_connected != connectedComputers;
  if (!changed)
    return;
  m_host = host;
  m_external = m_config.useExternalConfig();
  m_localName = localName;
  m_connected = connectedComputers;
  if (roleChanged || externalChanged || layoutChanged || (nameChanged && !m_editing)) {
    m_editing = false;
    reload();
    setEditing(false);
  }
  retranslate();
}

bool DeskWorkspace::isConnected(const Screen &screen) const
{
  if (m_connected.contains(screen.name(), Qt::CaseInsensitive))
    return true;
  for (const auto &alias : screen.aliases()) {
    if (m_connected.contains(alias, Qt::CaseInsensitive))
      return true;
  }
  return false;
}

void DeskWorkspace::rebuildOverview()
{
  while (auto *item = m_devices->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  for (int row = 0; row < m_devices->rowCount(); ++row)
    m_devices->setRowStretch(row, 0);
  for (int column = 0; column < m_devices->columnCount(); ++column)
    m_devices->setColumnStretch(column, 0);
  int minRow = m_rows, maxRow = 0, minColumn = m_columns, maxColumn = 0;
  for (int i = 0; i < m_screens.size(); ++i) {
    if (m_screens[i].isNull() || ((!m_host || m_config.useExternalConfig()) && !m_screens[i].isServer()))
      continue;
    minRow = qMin(minRow, i / m_columns);
    maxRow = qMax(maxRow, i / m_columns);
    minColumn = qMin(minColumn, i % m_columns);
    maxColumn = qMax(maxColumn, i % m_columns);
  }
  for (int i = 0; i < m_screens.size(); ++i) {
    const auto &screen = m_screens[i];
    if (screen.isNull() || ((!m_host || m_config.useExternalConfig()) && !screen.isServer()))
      continue;
    const bool local = screen.isServer();
    auto *button = new QToolButton(m_overview);
    button->setObjectName(QStringLiteral("deskComputer%1").arg(i));
    button->setProperty("deskComputer", true);
    button->setCheckable(true);
    button->setChecked(i == m_selected);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIcon(computerIcon(this));
    button->setIconSize(QSize(280, 200));
    button->setMinimumSize(0, 220);
    button->setMaximumSize(320, 270);
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    const auto name = local ? m_localName : screen.name();
    const auto state = local ? tr("This computer") : (isConnected(screen) ? tr("Connected") : tr("Not connected"));
    button->setText(fontMetrics().elidedText(name, Qt::ElideMiddle, 170) + QLatin1Char('\n') + state);
    button->setToolTip(name + QLatin1Char('\n') + state);
    button->setAccessibleName(name + QStringLiteral(", ") + state);
    m_devices->addWidget(button, i / m_columns - minRow, i % m_columns - minColumn, Qt::AlignCenter);
    connect(button, &QToolButton::clicked, this, [this, i] { selectComputer(m_selected == i ? -1 : i); });
  }
  for (int row = 0; row <= maxRow - minRow; ++row)
    m_devices->setRowStretch(row, 1);
  for (int column = 0; column <= maxColumn - minColumn; ++column)
    m_devices->setColumnStretch(column, 1);
}

void DeskWorkspace::selectComputer(int index)
{
  m_selected = index >= 0 && index < m_screens.size() && !m_screens[index].isNull() ? index : -1;
  for (auto *button : m_overview->findChildren<QToolButton *>())
    button->setChecked(button->objectName() == QStringLiteral("deskComputer%1").arg(m_selected));
  updateInspector();
}

QString DeskWorkspace::positionText(int index) const
{
  int host = -1;
  for (int i = 0; i < m_screens.size(); ++i) {
    if (m_screens[i].isServer())
      host = i;
  }
  if (host < 0 || index == host)
    return tr("This computer");
  const QPoint delta(index % m_columns - host % m_columns, index / m_columns - host / m_columns);
  QStringList parts;
  if (delta.x() != 0)
    parts << (delta.x() > 0 ? tr("Right") : tr("Left"));
  if (delta.y() != 0)
    parts << (delta.y() > 0 ? tr("Below") : tr("Above"));
  return parts.join(QStringLiteral(" · "));
}

void DeskWorkspace::updateInspector()
{
  const bool selected = m_selected >= 0 && m_selected < m_screens.size() && !m_screens[m_selected].isNull();
  m_inspector->setVisible(selected);
  m_inspectorScroll->setVisible(selected);
  if (!selected)
    return;
  const auto &screen = m_screens[m_selected];
  m_deviceName->setText(screen.isServer() ? m_localName : screen.name());
  m_deviceStatus->setText(
      screen.isServer() ? tr("This computer") : (isConnected(screen) ? tr("Connected") : tr("Not connected"))
  );
  const bool needsConnection = !screen.isServer() && !isConnected(screen);
  m_recoveryDetail->setText(
      m_sharing ? tr("On %1, select %2 as the host and connect.").arg(screen.name(), m_localName)
                : tr("Start sharing on this computer, then select this host and connect from %1.").arg(screen.name())
  );
  m_recoveryDetail->setVisible(needsConnection);
  m_connectionHelp->setVisible(needsConnection);
  m_position->setText(tr("Position: %1").arg(positionText(m_selected)));
  m_position->setVisible(m_host);
  m_directions->setVisible(canEdit() && m_editing);
  m_deviceSettings->setEnabled(canEdit());
  m_remove->setVisible(m_editing && canEdit() && !screen.isServer());
  m_inspectorScroll->setMinimumWidth(width() >= 900 ? 260 : 0);
  m_inspectorScroll->setMaximumWidth(width() >= 900 ? 260 : QWIDGETSIZE_MAX);
  m_inspectorScroll->setMaximumHeight(width() >= 900 ? QWIDGETSIZE_MAX : (m_editing ? 260 : 180));
}

void DeskWorkspace::setEditing(bool editing)
{
  m_editing = editing && canEdit();
  m_views->setCurrentWidget(m_editing ? static_cast<QWidget *>(m_editor) : m_overviewScroll);
  m_cancel->setVisible(m_editing);
  if (!m_editing)
    m_addForm->hide();
  m_feedback->hide();
  retranslate();
  updateInspector();
}

void DeskWorkspace::saveLayout()
{
  if (!canEdit())
    return;
  if (!(m_baseline == m_config.screens())) {
    m_feedback->setText(tr("The layout changed elsewhere. Cancel these edits and try again."));
    m_feedback->show();
    return;
  }
  m_config.setScreens(m_screens);
  m_config.commit();
  QStringList screenKeys;
  for (const auto &screen : m_screens) {
    if (!screen.isNull()) {
      screenKeys.append(QStringLiteral("screen_%1").arg(screen.name()));
    }
  }
  Settings::removeUnknownScreens(screenKeys);
  Settings::save(false);
  m_baseline = m_screens;
  setEditing(false);
  rebuildOverview();
  Q_EMIT configurationCommitted();
}

void DeskWorkspace::cancelEditing()
{
  reload();
  setEditing(false);
}

void DeskWorkspace::addComputer()
{
  if (!m_editing || !canEdit())
    return;
  const auto name = m_newName->text().trimmed();
  auto fail = [this](const QString &message) {
    m_feedback->setText(message);
    m_feedback->show();
  };
  if (name.isEmpty() || !validators::ComputerNameValidator({}).validate(name)) {
    fail(tr("Enter the other computer’s exact device name, without spaces or special symbols."));
    return;
  }
  int host = -1;
  for (int i = 0; i < m_screens.size(); ++i) {
    const auto &screen = m_screens[i];
    if (screen.name().compare(name, Qt::CaseInsensitive) == 0 || screen.aliases().contains(name, Qt::CaseInsensitive)) {
      fail(tr("That computer name or alias is already in this layout."));
      return;
    }
    if (screen.isServer())
      host = i;
  }
  const auto side = m_newSide->currentData().toPoint();
  const int column = host < 0 ? -1 : host % m_columns + side.x();
  const int row = host < 0 ? -1 : host / m_columns + side.y();
  if (!m_model->isInBounds(column, row) || !m_screens[row * m_columns + column].isNull()) {
    fail(tr("That position is occupied or outside the layout. Choose another side."));
    return;
  }
  const int destination = row * m_columns + column;
  m_screens[destination] = Screen(name);
  Q_EMIT m_model->dataChanged(m_model->index(row, column), m_model->index(row, column));
  m_editor->setCurrentIndex(m_model->index(row, column));
  selectComputer(destination);
  m_newName->clear();
  m_addForm->hide();
  fail(tr("Save the layout, then connect to this host from the other computer."));
}

void DeskWorkspace::moveComputer(const QPoint &direction)
{
  if (!canEdit() || m_selected < 0)
    return;
  setEditing(true);
  const int column = m_selected % m_columns + direction.x();
  const int row = m_selected / m_columns + direction.y();
  if (!m_model->isInBounds(column, row)) {
    m_feedback->setText(tr("That position is outside the layout."));
    m_feedback->show();
    return;
  }
  const auto destination = row * m_columns + column;
  qSwap(m_screens[m_selected], m_screens[destination]);
  Q_EMIT m_model->dataChanged(m_model->index(0, 0), m_model->index(m_rows - 1, m_columns - 1));
  m_editor->setCurrentIndex(m_model->index(row, column));
  selectComputer(destination);
}

void DeskWorkspace::editComputer()
{
  if (!canEdit() || m_selected < 0)
    return;
  setEditing(true);
  ScreenSettingsDialog dialog(this, &m_screens[m_selected], &m_screens);
  if (dialog.exec() == QDialog::Accepted) {
    Q_EMIT m_model->dataChanged(m_model->index(0, 0), m_model->index(m_rows - 1, m_columns - 1));
    updateInspector();
  }
}

void DeskWorkspace::removeComputer()
{
  if (!m_editing || !canEdit() || m_selected < 0 || m_screens[m_selected].isServer())
    return;
  const auto index = m_model->index(m_selected / m_columns, m_selected % m_columns);
  m_screens[m_selected] = Screen();
  Q_EMIT m_model->dataChanged(index, index);
  selectComputer(-1);
}

void DeskWorkspace::setStatus(const QString &title, const QString &detail, bool connected, bool sharing)
{
  if (m_sharing != sharing) {
    m_sharing = sharing;
    updateInspector();
  }
  m_status->setText(title);
  m_status->setAccessibleName(title);
  m_status->setProperty("connected", connected);
  m_status->setForegroundRole(QPalette::WindowText);
  m_detail->setText(detail);
  m_detail->setVisible(!connected);
}

void DeskWorkspace::retranslate()
{
  m_title->setText(tr("My desk"));
  m_edit->setText(
      m_editing ? tr("Save layout") : (m_host && !canEdit() ? tr("Open layout settings") : tr("Edit layout"))
  );
  m_edit->setVisible(m_host);
  m_cancel->setText(tr("Cancel"));
  m_add->setText(tr("Add computer"));
  m_deviceSettings->setText(tr("Computer settings"));
  m_connectionHelp->setText(tr("Show connection details"));
  m_remove->setText(tr("Remove computer"));
  m_advanced->setText(tr("More settings"));
  m_advanced->setVisible(m_editing || (m_host && m_config.useExternalConfig()));
  m_closeInspector->setAccessibleName(tr("Close computer details"));
  m_hint->setText(
      m_editing ? tr("Drag computers or select one and use the direction buttons. Save when finished.")
                : (m_host ? tr("Place computers like your desk. Move the pointer across a shared edge.")
                          : tr("Use your host’s keyboard and mouse here. Edit positions on the host."))
  );
  if (m_host && m_config.useExternalConfig())
    m_hint->setText(tr("Positions are managed by your external configuration file."));
  m_newName->setPlaceholderText(tr("Other computer’s device name"));
  m_newName->setAccessibleName(tr("Other computer’s device name"));
  m_newSide->setAccessibleName(tr("Position relative to this host"));
  const QStringList sides{tr("Right"), tr("Left"), tr("Above"), tr("Below")};
  for (int i = 0; i < sides.size(); ++i)
    m_newSide->setItemText(i, sides[i]);
  const QStringList moves{tr("Move up"), tr("Move left"), tr("Move right"), tr("Move down")};
  for (int i = 0; i < moves.size(); ++i) {
    auto *button = findChild<QToolButton *>(QStringLiteral("deskMove%1").arg(i));
    button->setAccessibleName(moves[i]);
    button->setToolTip(moves[i]);
  }
  m_confirmAdd->setText(tr("Add computer"));
  m_editor->setAccessibleName(tr("Computer positions. Select a computer to edit, move, or remove it."));
  if (!m_editing)
    rebuildOverview();
  updateInspector();
}

void DeskWorkspace::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  m_body->setDirection(width() >= 900 ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
  m_inspectorScroll->setMinimumWidth(width() >= 900 ? 260 : 0);
  m_inspectorScroll->setMaximumWidth(width() >= 900 ? 260 : QWIDGETSIZE_MAX);
  m_inspectorScroll->setMaximumHeight(width() >= 900 ? QWIDGETSIZE_MAX : (m_editing ? 260 : 180));
}

void DeskWorkspace::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  if (event->type() == QEvent::LanguageChange)
    retranslate();
}
