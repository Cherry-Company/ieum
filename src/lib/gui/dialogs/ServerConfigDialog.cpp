/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigDialog.h"
#include "ui_ServerConfigDialog.h"

#include "common/Constants.h"
#include "common/NetworkProtocol.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "dialogs/ActionDialog.h"
#include "dialogs/HotkeyDialog.h"
#include "dialogs/ScreenSettingsDialog.h"
#include "gui/StyleUtils.h"
#include "gui/validators/ComputerNameValidator.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QListWidget>
#include <QScrollArea>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>

#include <algorithm>

using enum ScreenConfig::SwitchCorner;

ServerConfigDialog::ServerConfigDialog(QWidget *parent, ServerConfig &config)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ServerConfigDialog>()},
      m_columns{Settings::value(Settings::Server::GridWidth).toInt()},
      m_rows{Settings::value(Settings::Server::GridHeight).toInt()},
      m_originalServerConfig(config),
      m_originalServerConfigIsExternal(config.useExternalConfig()),
      m_originalServerConfigUsesExternalFile(config.configFile()),
      m_serverConfig(config),
      m_screenSetupModel(m_serverConfig.screens(), m_columns, m_rows)
{
  ui->setupUi(this);
  ui->lineConfigFile->setStyleSheet(
      QStringLiteral("QLineEdit[configPathValid=\"false\"] { border: 1px solid #c42b1c; }")
  );

  loadFromConfig();

  ui->lblRemoveScreen->setPixmap(QIcon::fromTheme("user-trash").pixmap(QSize(64, 64)));
  ui->lblNewScreen->setEnabled(!model().isFull());
  ui->lblNewScreen->setPixmap(QIcon::fromTheme("video-display").pixmap(QSize(64, 64)));
  ui->btnBrowseConfigFile->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen));

  // force the first tab, since qt creator sets the active tab as the last one
  // the developer was looking at, and it's easy to accidentally save that.
  ui->tabWidget->setCurrentIndex(0);

  if (!deskflow::platform::isWindows())
    ui->cbWin32KeepForeground->setVisible(false);
  initConnections();
  createWorkspace();
  onChange();
}

ServerConfigDialog::~ServerConfigDialog() = default;

void ServerConfigDialog::createWorkspace()
{
  deskflow::gui::applyIeumDialogStyle(*this);
  setWindowTitle(tr("Your desk"));
  setMinimumSize(780, 560);
  resize(1000, 700);
  auto *root = qobject_cast<QVBoxLayout *>(layout());
  auto *heading = new QLabel(tr("Arrange computers like your desk"), this);
  heading->setProperty("sectionHeading", true);
  root->insertWidget(0, heading);
  m_navigation = new QListWidget(this);
  m_navigation->setObjectName(QStringLiteral("settingsNavigation"));
  m_navigation->setAccessibleName(tr("Desk settings categories"));
  m_navigation->setFixedWidth(180);
  const QStringList titles{
      tr("Computer positions"), tr("Shortcuts"), tr("Pointer & switching"), tr("Configuration file")
  };
  for (const auto &title : titles) {
    auto *item = new QListWidgetItem(title, m_navigation);
    item->setSizeHint(QSize(160, 48));
  }
  root->removeWidget(ui->tabWidget);
  auto *body = new QHBoxLayout;
  body->addWidget(m_navigation);
  body->addWidget(ui->tabWidget, 1);
  root->insertLayout(1, body, 1);
  ui->tabWidget->tabBar()->hide();
  connect(m_navigation, &QListWidget::currentRowChanged, ui->tabWidget, &QTabWidget::setCurrentIndex);
  m_navigation->setCurrentRow(serverConfig().useExternalConfig() ? 3 : 0);

  for (int index : {1, 2, 3}) {
    auto *page = ui->tabWidget->widget(index);
    auto *contents = new QWidget;
    contents->setLayout(page->layout());
    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(contents);
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scroll);
  }

  ui->lblNewScreen->hide();
  ui->lblRemoveScreen->hide();
  ui->label_2->hide();
  auto *computers = qobject_cast<QVBoxLayout *>(ui->tabComputers->layout());
  computers->removeItem(ui->layoutComputers);
  delete ui->layoutComputers;
  auto *instructions = new QLabel(
      tr("Add the other computer using its exact “This device” name. Choose its side, then move the pointer across "
         "that edge. "
         "You can drag computers or select one and use the arrow buttons."),
      this
  );
  instructions->setWordWrap(true);
  computers->insertWidget(0, instructions);
  auto *form = new QHBoxLayout;
  m_newComputerName = new QLineEdit(this);
  m_newComputerName->setObjectName(QStringLiteral("newComputerName"));
  m_newComputerName->setPlaceholderText(tr("Other computer’s device name"));
  m_newComputerName->setAccessibleName(tr("Other computer’s device name"));
  m_newComputerSide = new QComboBox(this);
  m_newComputerSide->setObjectName(QStringLiteral("newComputerSide"));
  m_newComputerSide->setAccessibleName(tr("Position relative to this host"));
  m_newComputerSide->addItem(tr("Right of this host"), QPoint(1, 0));
  m_newComputerSide->addItem(tr("Left of this host"), QPoint(-1, 0));
  m_newComputerSide->addItem(tr("Above this host"), QPoint(0, -1));
  m_newComputerSide->addItem(tr("Below this host"), QPoint(0, 1));
  m_addComputer = new QPushButton(tr("Add computer"), this);
  m_addComputer->setObjectName(QStringLiteral("addComputer"));
  form->addWidget(m_newComputerName, 1);
  form->addWidget(m_newComputerSide);
  form->addWidget(m_addComputer);
  computers->insertLayout(1, form);
  m_layoutFeedback = new QLabel(this);
  m_layoutFeedback->setObjectName(QStringLiteral("layoutFeedback"));
  m_layoutFeedback->setWordWrap(true);
  computers->insertWidget(2, m_layoutFeedback);
  auto *actions = new QHBoxLayout;
  m_editComputer = new QPushButton(tr("Computer settings"), this);
  m_editComputer->setObjectName(QStringLiteral("editComputer"));
  m_removeComputer = new QPushButton(tr("Remove computer"), this);
  m_removeComputer->setObjectName(QStringLiteral("removeComputer"));
  actions->addWidget(m_editComputer);
  actions->addWidget(m_removeComputer);
  actions->addStretch();
  const QList<QPoint> directions{QPoint(-1, 0), QPoint(0, -1), QPoint(0, 1), QPoint(1, 0)};
  const QStringList names{tr("Move left"), tr("Move up"), tr("Move down"), tr("Move right")};
  for (int i = 0; i < directions.size(); ++i) {
    auto *button = new QToolButton(this);
    button->setText(QStringList{QStringLiteral("←"), QStringLiteral("↑"), QStringLiteral("↓"), QStringLiteral("→")}[i]);
    button->setToolTip(names[i]);
    button->setAccessibleName(names[i]);
    button->setAutoRaise(false);
    button->setMinimumSize(36, 32);
    connect(button, &QToolButton::clicked, this, [this, direction = directions[i]] {
      moveSelectedComputer(direction);
    });
    actions->addWidget(button);
  }
  computers->addLayout(actions);
  ui->screenSetupView->setAccessibleName(tr("Computer positions. Select a computer to edit, move, or remove it."));
  connect(m_addComputer, &QPushButton::clicked, this, &ServerConfigDialog::addComputerFromForm);
  connect(m_newComputerName, &QLineEdit::returnPressed, this, &ServerConfigDialog::addComputerFromForm);
  connect(m_editComputer, &QPushButton::clicked, this, &ServerConfigDialog::editSelectedComputer);
  connect(m_removeComputer, &QPushButton::clicked, this, &ServerConfigDialog::removeSelectedComputer);
  connect(
      ui->screenSetupView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      &ServerConfigDialog::updateComputerActions
  );
  connect(&m_screenSetupModel, &ScreenSetupModel::screensChanged, this, &ServerConfigDialog::updateComputerActions);
  updateComputerActions();
}

void ServerConfigDialog::updateComputerActions()
{
  const auto *screen = model().screenAt(ui->screenSetupView->currentIndex());
  const bool selected = screen != nullptr && !screen->isNull();
  m_editComputer->setEnabled(selected);
  m_removeComputer->setEnabled(selected && !screen->isServer());
  m_addComputer->setEnabled(!model().isFull() && !serverConfig().useExternalConfig());
  for (int i = 0; i < m_navigation->count(); ++i) {
    auto *item = m_navigation->item(i);
    item->setFlags(ui->tabWidget->isTabEnabled(i) ? Qt::ItemIsSelectable | Qt::ItemIsEnabled : Qt::NoItemFlags);
  }
}

void ServerConfigDialog::addComputerFromForm()
{
  const auto name = m_newComputerName->text().trimmed();
  if (name.isEmpty() || !validators::ComputerNameValidator({}).validate(name)) {
    m_layoutFeedback->setText(
        tr("Use the exact device name shown on the other computer. Spaces and special symbols are not allowed.")
    );
    return;
  }
  for (const auto &screen : model().m_Screens) {
    if (screen.name().compare(name, Qt::CaseInsensitive) == 0 || screen.aliases().contains(name, Qt::CaseInsensitive)) {
      m_layoutFeedback->setText(tr("That computer name or alias is already in this layout."));
      return;
    }
  }
  for (int row = 0; row < m_rows; ++row) {
    for (int column = 0; column < m_columns; ++column) {
      if (!model().screenAt(column, row)->isServer()) {
        continue;
      }
      const auto position = QPoint(column, row) + m_newComputerSide->currentData().toPoint();
      auto *destination = model().screenAt(position.x(), position.y());
      if (destination == nullptr || !destination->isNull()) {
        m_layoutFeedback->setText(
            tr("That position is occupied or outside the grid. Choose another side, or move a computer first.")
        );
        return;
      }
      *destination = Screen(name);
      const auto index = model().index(position.y(), position.x());
      Q_EMIT model().dataChanged(index, index);
      Q_EMIT model().screensChanged();
      ui->screenSetupView->setCurrentIndex(index);
      m_newComputerName->clear();
      m_layoutFeedback->setText(tr("%1 added. Save, then connect to this host from that computer.").arg(name));
      return;
    }
  }
  m_layoutFeedback->setText(
      tr("This host is missing from the layout. Restore its computer entry before adding another computer.")
  );
}

void ServerConfigDialog::editSelectedComputer()
{
  auto *screen = model().screenAt(ui->screenSetupView->currentIndex());
  if (screen == nullptr || screen->isNull()) {
    return;
  }
  ScreenSettingsDialog dialog(this, screen, &model().m_Screens);
  if (dialog.exec() == QDialog::Accepted) {
    Q_EMIT model().dataChanged(ui->screenSetupView->currentIndex(), ui->screenSetupView->currentIndex());
    Q_EMIT model().screensChanged();
  }
}

void ServerConfigDialog::removeSelectedComputer()
{
  auto *screen = model().screenAt(ui->screenSetupView->currentIndex());
  if (screen == nullptr || screen->isNull() || screen->isServer()) {
    return;
  }
  *screen = Screen();
  Q_EMIT model().dataChanged(ui->screenSetupView->currentIndex(), ui->screenSetupView->currentIndex());
  Q_EMIT model().screensChanged();
}

void ServerConfigDialog::moveSelectedComputer(const QPoint &direction)
{
  const auto source = ui->screenSetupView->currentIndex();
  auto *screen = model().screenAt(source);
  auto *destination = model().screenAt(source.column() + direction.x(), source.row() + direction.y());
  if (screen == nullptr || screen->isNull() || destination == nullptr) {
    return;
  }
  std::swap(*screen, *destination);
  const auto target = model().index(source.row() + direction.y(), source.column() + direction.x());
  Q_EMIT model().dataChanged(model().index(0, 0), model().index(m_rows - 1, m_columns - 1));
  Q_EMIT model().screensChanged();
  ui->screenSetupView->setCurrentIndex(target);
}

bool ServerConfigDialog::addClient(const QString &clientName)
{
  return addComputer(clientName, true);
}

void ServerConfigDialog::accept()
{
  setServerConfigFile(ui->lineConfigFile->text());
  if (!isExternalConfigFileValid()) {
    ui->lineConfigFile->setFocus(Qt::OtherFocusReason);
    return;
  }

  // now that the dialog has been accepted, copy the new server config to the
  // original one, which is a reference to the one in MainWindow.
  setOriginalServerConfig(serverConfig());
  Settings::setValue(Settings::Server::Protocol, networkProtocolToOption(m_protocol));
  Settings::setValue(Settings::Server::EnableClipboard, m_enableClipboard);
  Settings::setValue(Settings::Server::ClipboardSize, m_clipboardSize);
  Settings::setValue(Settings::Server::EnableHeatbeat, m_enableHeartbeat);
  Settings::setValue(Settings::Server::Heartbeat, m_heartbeatRate);
  Settings::setValue(Settings::Server::EnableSwitchDelay, m_enableSwitchDelay);
  Settings::setValue(Settings::Server::SwitchDelay, m_switchDelay);
  Settings::setValue(Settings::Server::DefaultLockToComputerState, m_defaultLockToComputerState);
  Settings::setValue(Settings::Server::DisableLockToComputer, m_disableLockToComputer);
  Settings::setValue(Settings::Server::EnableSwitchDoubleTap, m_enableSwitchDoubleTap);
  Settings::setValue(Settings::Server::SwitchDoubleTap, m_switchDoubleTap);
  Settings::setValue(Settings::Server::RelativeMouseMoves, m_relativeMouseMoves);
  Settings::setValue(Settings::Server::RemotePointerSpeed, m_remotePointerSpeed);
  Settings::setValue(Settings::Server::AutoLockFullscreen, m_autoLockFullscreen);
  Settings::setValue(Settings::Server::Win32KeepForeground, m_win32keepForeground);

  QStringList screenNames;
  const auto screenList = m_screenSetupModel.m_Screens;
  for (const auto &screen : screenList) {
    const auto &screenName = screen.name();
    if (screenName.isEmpty())
      continue;
    screenNames.append(QStringLiteral("screen_%1").arg(screenName));
    Settings::setValue(Settings::Screen::Aliases.arg(screenName), screen.aliases());
  }
  Settings::removeUnknownScreens(screenNames);
  QDialog::accept();
}

void ServerConfigDialog::reject()
{
  serverConfig().setUseExternalConfig(m_originalServerConfigIsExternal);
  serverConfig().setConfigFile(m_originalServerConfigUsesExternalFile);

  QDialog::reject();
}

void ServerConfigDialog::addHotkey()
{
  Hotkey hotkey;
  HotkeyDialog dlg(this, hotkey);
  if (dlg.exec() == QDialog::Accepted) {
    serverConfig().hotkeys().append(hotkey);
    ui->listHotkeys->addItem(hotkey.text());
    onChange();
  }
}

void ServerConfigDialog::editHotkey()
{
  int row = ui->listHotkeys->currentRow();
  if (row < 0 || row >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to editing out of bounds hotkey row: " << row;
    return;
  }

  Hotkey &hotkey = serverConfig().hotkeys()[row];
  HotkeyDialog dlg(this, hotkey);
  if (dlg.exec() == QDialog::Accepted) {
    ui->listHotkeys->currentItem()->setText(hotkey.text());
    onChange();
  }
}

void ServerConfigDialog::removeHotkey()
{
  int row = ui->listHotkeys->currentRow();
  if (row < 0 || row >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to remove out of bounds hotkey row: " << row;
    return;
  }

  serverConfig().hotkeys().removeAt(row);
  ui->listActions->clear();
  delete ui->listHotkeys->item(row);
  onChange();
}

void ServerConfigDialog::listHotkeysSelectionChanged(const QItemSelection &selected, const QItemSelection &)
{
  bool itemsSelected = !selected.isEmpty();
  ui->btnEditHotkey->setEnabled(itemsSelected);
  ui->btnRemoveHotkey->setEnabled(itemsSelected);
  ui->btnNewAction->setEnabled(itemsSelected);

  if (itemsSelected && !serverConfig().hotkeys().isEmpty()) {
    ui->listActions->clear();
    const Hotkey &hotkey = serverConfig().hotkeys().at(selected.indexes().first().row());
    for (const Action &action : hotkey.actions())
      ui->listActions->addItem(action.text());
  }
}

void ServerConfigDialog::addAction()
{
  int row = ui->listHotkeys->currentRow();
  if (row < 0 || row >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to add action to out of bounds hotkey row: " << row;
    return;
  }

  Hotkey &hotkey = serverConfig().hotkeys()[row];
  Action action;
  ActionDialog dlg(this, serverConfig(), hotkey, action);
  if (dlg.exec() == QDialog::Accepted) {
    hotkey.actions().append(action);
    ui->listActions->addItem(action.text());
    onChange();
  }
}

void ServerConfigDialog::editAction()
{
  int hotkeyRow = ui->listHotkeys->currentRow();
  if (hotkeyRow < 0 || hotkeyRow >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to edit action from out of bounds hotkey row: " << hotkeyRow;
    return;
  }
  Hotkey &hotkey = serverConfig().hotkeys()[hotkeyRow];

  int actionRow = ui->listActions->currentRow();
  if (actionRow < 0 || actionRow >= hotkey.actions().size()) {
    qDebug() << "Attempt to remove out of bounds action row: " << actionRow;
    return;
  }
  Action &action = hotkey.actions()[actionRow];

  ActionDialog dlg(this, serverConfig(), hotkey, action);
  if (dlg.exec() == QDialog::Accepted) {
    ui->listActions->currentItem()->setText(action.text());
    onChange();
  }
}

void ServerConfigDialog::removeAction()
{
  int hotkeyRow = ui->listHotkeys->currentRow();
  if (hotkeyRow < 0 || hotkeyRow >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to remove action from out of bounds hotkey row: " << hotkeyRow;
    return;
  }
  Hotkey &hotkey = serverConfig().hotkeys()[hotkeyRow];

  int actionRow = ui->listActions->currentRow();
  if (actionRow < 0 || actionRow >= hotkey.actions().size()) {
    qDebug() << "Attempt to remove out of bounds action row: " << actionRow;
    return;
  }

  hotkey.actions().removeAt(actionRow);
  delete ui->listActions->currentItem();
  onChange();
}

void ServerConfigDialog::toggleClipboard(bool enabled)
{
  if (m_enableClipboard == enabled)
    return;

  m_enableClipboard = enabled;

  ui->sbClipboardSizeLimit->setEnabled(enabled);
  if (enabled && !ui->sbClipboardSizeLimit->value()) {
    m_clipboardSize = Settings::defaultValue(Settings::Server::ClipboardSize).toUInt();
    ui->sbClipboardSizeLimit->setValue(m_clipboardSize ? m_clipboardSize : 1);
  }
  onChange();
}

void ServerConfigDialog::setClipboardLimit(int limit)
{
  if (m_clipboardSize == limit)
    return;

  m_clipboardSize = limit;
  onChange();
}

void ServerConfigDialog::toggleHeartbeat(bool enabled)
{
  m_enableHeartbeat = enabled;
  ui->sbHeartbeat->setEnabled(enabled);
  onChange();
}

void ServerConfigDialog::setHeartbeat(int rate)
{
  if (rate == m_heartbeatRate)
    return;
  m_heartbeatRate = rate;
  onChange();
}

void ServerConfigDialog::toggleRelativeMouseMoves(bool enabled)
{
  if (m_relativeMouseMoves == enabled)
    return;
  m_relativeMouseMoves = enabled;
  onChange();
}

void ServerConfigDialog::setRemotePointerSpeed(int speed)
{
  if (m_remotePointerSpeed == speed)
    return;
  m_remotePointerSpeed = speed;
  onChange();
}

void ServerConfigDialog::toggleProtocol()
{
  m_protocol = ui->rbProtocolBarrier->isChecked() ? NetworkProtocol::Barrier : NetworkProtocol::Synergy;
  onChange();
}

void ServerConfigDialog::listActionsSelectionChanged(const QItemSelection &selected, const QItemSelection &)
{
  bool enabled = !selected.isEmpty();
  ui->btnEditAction->setEnabled(enabled);
  ui->btnRemoveAction->setEnabled(enabled);
}

void ServerConfigDialog::toggleSwitchDoubleTap(bool enable)
{
  m_enableSwitchDoubleTap = enable;
  ui->sbSwitchDoubleTap->setEnabled(enable);
  onChange();
}

void ServerConfigDialog::setSwitchDoubleTap(int within)
{
  if (m_switchDoubleTap == within)
    return;
  m_switchDoubleTap = within;
  onChange();
}

void ServerConfigDialog::toggleSwitchDelay(bool enable)
{
  m_enableSwitchDelay = enable;
  ui->sbSwitchDelay->setEnabled(enable);
  onChange();
}

void ServerConfigDialog::setSwitchDelay(int delay)
{
  if (m_switchDelay == delay)
    return;
  m_switchDelay = delay;
  onChange();
}

void ServerConfigDialog::toggleDefaultLockToComputerState(bool state)
{
  if (m_defaultLockToComputerState == state)
    return;
  m_defaultLockToComputerState = state;
  onChange();
}

void ServerConfigDialog::toggleLockToComputer(bool disabled)
{
  if (m_disableLockToComputer == disabled)
    return;
  m_disableLockToComputer = disabled;
  onChange();
}

void ServerConfigDialog::toggleAutoLockFullscreen(bool enabled)
{
  if (m_autoLockFullscreen == enabled)
    return;
  m_autoLockFullscreen = enabled;
  onChange();
}

void ServerConfigDialog::toggleWin32Foreground(bool enabled)
{
  if (m_win32keepForeground == enabled)
    return;
  m_win32keepForeground = enabled;
  onChange();
}

void ServerConfigDialog::addClient()
{
  addComputer("", false);
}

void ServerConfigDialog::onScreenRemoved()
{
  ui->lblNewScreen->setEnabled(true);
  onChange();
}

void ServerConfigDialog::toggleExternalConfig(bool checked)
{
  ui->widgetExternalConfigControls->setEnabled(checked);
  ui->tabWidget->setTabEnabled(0, !checked);
  ui->tabWidget->setTabEnabled(1, !checked);
  serverConfig().setUseExternalConfig(checked);
  onChange();
}

bool ServerConfigDialog::browseConfigFile()
{
  //: %1 is replaced with the application names
  //: (*.conf) and (*.*) should not be translated
  const auto deskflowConfigFilter = tr("%1 Configurations (*.conf);;All files (*.*)");

  const auto fileName = chooseConfigFile(deskflowConfigFilter.arg(kAppName));

  if (!fileName.isEmpty()) {
    ui->lineConfigFile->setText(fileName);
    return true;
  }

  return false;
}

QString ServerConfigDialog::chooseConfigFile(const QString &filter)
{
  return QFileDialog::getOpenFileName(this, tr("Browse for a config file"), {}, filter);
}

void ServerConfigDialog::loadFromConfig()
{
  m_protocol = Settings::networkProtocol();
  ui->rbProtocolSynergy->setChecked(m_protocol == NetworkProtocol::Synergy);
  ui->rbProtocolBarrier->setChecked(m_protocol == NetworkProtocol::Barrier);

  ui->lineConfigFile->setText(serverConfig().configFile());

  m_enableHeartbeat = Settings::value(Settings::Server::EnableHeatbeat).toBool();
  ui->cbHeartbeat->setChecked(m_enableHeartbeat);
  ui->sbHeartbeat->setEnabled(ui->cbHeartbeat->isChecked());

  m_heartbeatRate = Settings::value(Settings::Server::Heartbeat).toInt();
  ui->sbHeartbeat->setValue(m_heartbeatRate);

  m_relativeMouseMoves = Settings::value(Settings::Server::RelativeMouseMoves).toBool();
  ui->cbRelativeMouseMoves->setChecked(m_relativeMouseMoves);

  m_remotePointerSpeed = std::clamp(Settings::value(Settings::Server::RemotePointerSpeed).toInt(), 25, 400);
  ui->sliderRemotePointerSpeed->setValue(m_remotePointerSpeed);
  ui->sbRemotePointerSpeed->setValue(m_remotePointerSpeed);

  m_autoLockFullscreen = Settings::value(Settings::Server::AutoLockFullscreen).toBool();
  ui->cbAutoLockFullscreen->setChecked(m_autoLockFullscreen);

  m_win32keepForeground = Settings::value(Settings::Server::Win32KeepForeground).toBool();
  ui->cbWin32KeepForeground->setChecked(m_win32keepForeground);

  m_enableSwitchDelay = Settings::value(Settings::Server::EnableSwitchDelay).toBool();
  ui->cbSwitchDelay->setChecked(m_enableSwitchDelay);
  ui->sbSwitchDelay->setEnabled(ui->cbSwitchDelay->isChecked());

  m_switchDelay = Settings::value(Settings::Server::SwitchDelay).toInt();
  ui->sbSwitchDelay->setValue(m_switchDelay);

  m_enableSwitchDoubleTap = Settings::value(Settings::Server::EnableSwitchDoubleTap).toBool();
  ui->cbSwitchDoubleTap->setChecked(m_enableSwitchDoubleTap);
  ui->sbSwitchDoubleTap->setEnabled(ui->cbSwitchDoubleTap->isChecked());

  m_switchDoubleTap = Settings::value(Settings::Server::SwitchDoubleTap).toInt();
  ui->sbSwitchDoubleTap->setValue(m_switchDoubleTap);

  ui->groupExternalConfig->setChecked(serverConfig().useExternalConfig());

  ui->widgetExternalConfigControls->setEnabled(ui->groupExternalConfig->isChecked());
  toggleExternalConfig(ui->groupExternalConfig->isChecked());

  m_defaultLockToComputerState = Settings::value(Settings::Server::DefaultLockToComputerState).toBool();
  ui->cbDefaultLockToComputerState->setChecked(m_defaultLockToComputerState);

  m_disableLockToComputer = Settings::value(Settings::Server::DisableLockToComputer).toBool();
  ui->cbDisableLockToComputer->setChecked(m_disableLockToComputer);

  m_enableClipboard = Settings::value(Settings::Server::EnableClipboard).toBool();
  ui->cbEnableClipboard->setChecked(m_enableClipboard);
  ui->sbClipboardSizeLimit->setEnabled(m_enableClipboard);

  m_clipboardSize = Settings::value(Settings::Server::ClipboardSize).toUInt();
  ui->sbClipboardSizeLimit->setValue(m_clipboardSize);

  ui->listHotkeys->clear();
  for (const Hotkey &hotkey : std::as_const(serverConfig().hotkeys()))
    ui->listHotkeys->addItem(hotkey.text());

  ui->screenSetupView->setModel(&m_screenSetupModel);

  auto &screens = serverConfig().screens();
  auto server = std::ranges::find_if(screens, [this](const Screen &screen) {
    return (screen.name() == serverConfig().getServerName());
  });

  if (server == screens.end()) {
    Screen serverScreen(serverConfig().getServerName());
    serverScreen.markAsServer();
    if (auto *centerScreen = model().screenAt(m_columns / 2, m_rows / 2); centerScreen != nullptr)
      *centerScreen = serverScreen;
  } else {
    server->markAsServer();
  }
}

void ServerConfigDialog::initConnections() const
{
  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ServerConfigDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ServerConfigDialog::reject);
  connect(ui->lblRemoveScreen, &TrashScreenWidget::screenRemoved, this, &ServerConfigDialog::onScreenRemoved);
  connect(ui->btnNewHotkey, &QPushButton::clicked, this, &ServerConfigDialog::addHotkey);
  connect(ui->btnEditHotkey, &QPushButton::clicked, this, &ServerConfigDialog::editHotkey);
  connect(ui->btnRemoveHotkey, &QPushButton::clicked, this, &ServerConfigDialog::removeHotkey);
  connect(ui->listHotkeys, &QListView::doubleClicked, this, &ServerConfigDialog::editHotkey);
  connect(
      ui->listHotkeys->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      &ServerConfigDialog::listHotkeysSelectionChanged
  );

  connect(ui->btnNewAction, &QPushButton::clicked, this, &ServerConfigDialog::addAction);
  connect(ui->btnEditAction, &QPushButton::clicked, this, &ServerConfigDialog::editAction);
  connect(ui->btnRemoveAction, &QPushButton::clicked, this, &ServerConfigDialog::removeAction);
  connect(ui->listActions, &QListView::doubleClicked, this, &ServerConfigDialog::editAction);
  connect(
      ui->listActions->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      &ServerConfigDialog::listActionsSelectionChanged
  );

  connect(ui->rbProtocolBarrier, &QRadioButton::toggled, this, &ServerConfigDialog::toggleProtocol);
  connect(ui->cbHeartbeat, &QCheckBox::toggled, this, &ServerConfigDialog::toggleHeartbeat);
  connect(ui->sbHeartbeat, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setHeartbeat);
  connect(ui->cbWin32KeepForeground, &QCheckBox::toggled, this, &ServerConfigDialog::toggleWin32Foreground);
  connect(ui->cbSwitchDelay, &QCheckBox::toggled, this, &ServerConfigDialog::toggleSwitchDelay);
  connect(ui->sbSwitchDelay, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setSwitchDelay);
  connect(ui->cbSwitchDoubleTap, &QCheckBox::toggled, this, &ServerConfigDialog::toggleSwitchDoubleTap);
  connect(
      ui->sbSwitchDoubleTap, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setSwitchDoubleTap
  );

  connect(ui->cbRelativeMouseMoves, &QCheckBox::toggled, this, &ServerConfigDialog::toggleRelativeMouseMoves);
  connect(ui->sliderRemotePointerSpeed, &QSlider::valueChanged, ui->sbRemotePointerSpeed, &QSpinBox::setValue);
  connect(
      ui->sbRemotePointerSpeed, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderRemotePointerSpeed,
      &QSlider::setValue
  );
  connect(
      ui->sbRemotePointerSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this,
      &ServerConfigDialog::setRemotePointerSpeed
  );
  connect(ui->cbEnableClipboard, &QCheckBox::toggled, this, &ServerConfigDialog::toggleClipboard);
  connect(ui->btnBrowseConfigFile, &QPushButton::clicked, this, &ServerConfigDialog::browseConfigFile);
  connect(ui->groupExternalConfig, &QGroupBox::toggled, this, &ServerConfigDialog::toggleExternalConfig);
  connect(ui->lineConfigFile, &QLineEdit::textChanged, this, &ServerConfigDialog::setServerConfigFile);

  connect(
      ui->sbClipboardSizeLimit, QOverload<int>::of(&QSpinBox::valueChanged), this,
      &ServerConfigDialog::setClipboardLimit
  );
  connect(
      ui->cbDefaultLockToComputerState, &QCheckBox::toggled, this, &ServerConfigDialog::toggleDefaultLockToComputerState
  );
  connect(ui->cbDisableLockToComputer, &QCheckBox::toggled, this, &ServerConfigDialog::toggleLockToComputer);
  connect(ui->cbAutoLockFullscreen, &QCheckBox::toggled, this, &ServerConfigDialog::toggleAutoLockFullscreen);
  connect(&m_screenSetupModel, &ScreenSetupModel::screensChanged, this, &ServerConfigDialog::onChange);
}

bool ServerConfigDialog::isExternalConfigFileValid() const
{
  return !ui->groupExternalConfig->isChecked() || QFileInfo(ui->lineConfigFile->text()).isFile();
}

void ServerConfigDialog::setServerConfigFile(const QString &path)
{
  serverConfig().setConfigFile(path);
  onChange();
}

void ServerConfigDialog::updateConfigFileValidation() const
{
  const auto isValid = isExternalConfigFileValid();
  const auto errorMessage = isValid ? QString() : tr("Select an existing configuration file.");

  if (!ui->lineConfigFile->property("configPathValid").isValid() ||
      ui->lineConfigFile->property("configPathValid").toBool() != isValid) {
    ui->lineConfigFile->setProperty("configPathValid", isValid);
    ui->lineConfigFile->style()->unpolish(ui->lineConfigFile);
    ui->lineConfigFile->style()->polish(ui->lineConfigFile);
  }

  ui->lineConfigFile->setToolTip(errorMessage);
  ui->lineConfigFile->setAccessibleDescription(errorMessage);
}

bool ServerConfigDialog::addComputer(const QString &clientName, bool doSilent)
{
  bool isAccepted = false;
  Screen newScreen(clientName);

  if (ScreenSettingsDialog dlg(this, &newScreen, &model().m_Screens); doSilent || dlg.exec() == QDialog::Accepted) {
    model().addScreen(newScreen);
    isAccepted = true;
  }

  ui->lblNewScreen->setEnabled(!model().isFull());
  return isAccepted;
}

void ServerConfigDialog::onChange()
{
  if (m_navigation != nullptr) {
    updateComputerActions();
  }
  updateConfigFileValidation();

  bool isAppConfigDataEqual =
      m_originalServerConfigIsExternal == serverConfig().useExternalConfig() &&
      m_originalServerConfigUsesExternalFile == serverConfig().configFile() &&
      m_protocol == Settings::networkProtocol() &&
      m_enableClipboard == Settings::value(Settings::Server::EnableClipboard).toBool() &&
      m_clipboardSize == Settings::value(Settings::Server::ClipboardSize).toUInt() &&
      m_enableHeartbeat == Settings::value(Settings::Server::EnableHeatbeat).toBool() &&
      m_heartbeatRate == Settings::value(Settings::Server::Heartbeat).toInt() &&
      m_enableSwitchDelay == Settings::value(Settings::Server::EnableSwitchDelay).toBool() &&
      m_switchDelay == Settings::value(Settings::Server::SwitchDelay).toInt() &&
      m_enableSwitchDoubleTap == Settings::value(Settings::Server::EnableSwitchDoubleTap).toBool() &&
      m_switchDoubleTap == Settings::value(Settings::Server::SwitchDoubleTap).toInt() &&
      m_relativeMouseMoves == Settings::value(Settings::Server::RelativeMouseMoves).toBool() &&
      m_remotePointerSpeed == Settings::value(Settings::Server::RemotePointerSpeed).toInt() &&
      m_autoLockFullscreen == Settings::value(Settings::Server::AutoLockFullscreen).toBool() &&
      m_win32keepForeground == Settings::value(Settings::Server::Win32KeepForeground).toBool() &&
      m_disableLockToComputer == Settings::value(Settings::Server::DisableLockToComputer).toBool() &&
      m_defaultLockToComputerState == Settings::value(Settings::Server::DefaultLockToComputerState).toBool();
  ui->buttonBox->button(QDialogButtonBox::Ok)
      ->setEnabled(
          isExternalConfigFileValid() && (!isAppConfigDataEqual || !(m_originalServerConfig == m_serverConfig))
      );
}
