/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsDialog.h"
#include "common/LogLevel.h"
#include "common/PlatformInfo.h"
#include "common/UrlConstants.h"
#include "ui_SettingsDialog.h"

#include "common/I18N.h"
#include "common/Settings.h"
#include "gui/ExternalUrlLauncher.h"
#include "gui/Messages.h"
#include "gui/StartupManager.h"
#include "gui/StyleUtils.h"
#include "gui/TlsUtility.h"
#include "gui/core/NetworkMonitor.h"
#include "gui/dialogs/ProLicenseUiPolicy.h"
#include "licensing/ProEarlyAccessClaim.h"
#include "licensing/ProLicense.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStringDecoder>
#include <QTabBar>
#include <QVBoxLayout>

using namespace deskflow::gui;

namespace {

//! Select the item carrying \p data, falling back to \p fallback.
/*!
findData() answers -1 for a value this build does not offer, and that would
leave the combo blank and write the empty selection straight back on accept.
*/
void selectComboData(QComboBox *combo, const QString &data, const QString &fallback)
{
  auto index = combo->findData(data);
  if (index < 0) {
    index = combo->findData(fallback);
  }
  combo->setCurrentIndex(index < 0 ? 0 : index);
}

FileTransferPlatformSupport fileTransferPlatformSupport() noexcept
{
  if (deskflow::platform::isWindows()) {
    return FileTransferPlatformSupport::Windows;
  }
  if (deskflow::platform::isMac()) {
    return FileTransferPlatformSupport::MacOS;
  }
  return FileTransferPlatformSupport::Unsupported;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent, const ServerConfig &serverConfig)
    : QDialog(parent),
      ui{std::make_unique<Ui::SettingsDialog>()},
      m_serverConfig(serverConfig),
      m_tailscaleIntegration{new deskflow::network::TailscaleIntegration(this)}
{

  ui->setupUi(this);
  ensureProEarlyAccessClaim();
  applyIeumDialogStyle(*this);

  ui->comboCjkRawScancode->addItem(tr("Automatic"), QStringLiteral("auto"));
  ui->comboCjkRawScancode->addItem(tr("Always"), QStringLiteral("on"));
  ui->comboCjkRawScancode->addItem(tr("Never"), QStringLiteral("off"));
  ui->comboEnterScreenLang->addItem(tr("Keep remote state"), QStringLiteral("keep"));
  ui->comboEnterScreenLang->addItem(tr("Force English"), QStringLiteral("force-en"));
  ui->comboEnterScreenLang->addItem(tr("Follow this computer"), QStringLiteral("follow-server"));

  // these are enabled by the control next to them
  ui->lineCommandEnter->setEnabled(false);
  ui->lineCommandExit->setEnabled(false);

  // set up the language combo
  I18N::reDetectLanguages();
  ui->comboLanguage->addItems(I18N::detectedLanguages());
  ui->comboLanguage->setCurrentText(I18N::toNativeName(I18N::currentLanguage()));

  updateText();

  ui->comboTlsKeyLength->setItemIcon(0, QIcon::fromTheme(QStringLiteral("security-medium")));
  ui->comboTlsKeyLength->setItemIcon(1, QIcon::fromTheme(QIcon::ThemeIcon::SecurityHigh));
  ui->lblTlsCertInfo->setFixedSize(28, 28);

  ui->rbIconMono->setIcon(QIcon::fromTheme(QStringLiteral("%1-symbolic").arg(kRevFqdnName)));
  ui->rbIconColorful->setIcon(QIcon::fromTheme(kRevFqdnName));

  // force the first tab, since qt creator sets the active tab as the last one
  // the developer was looking at, and it's easy to accidentally save that.
  ui->tabWidget->setCurrentIndex(0);

  // Populate the list of IP addresses
  const auto validAddresses = NetworkMonitor::validAddresses();
  for (const auto &address : validAddresses) {
    QString ipString = address;
    if (ui->comboInterface->findText(ipString) == -1) {
      ui->comboInterface->addItem(ipString, ipString);
    }
  }

  if (const auto interface = Settings::value(Settings::Core::Interface).toString();
      !interface.isEmpty() && (ui->comboInterface->findData(interface) == -1)) {
    ui->comboInterface->addItem(interface, interface);
  }

  connect(
      m_tailscaleIntegration, &deskflow::network::TailscaleIntegration::queryStarted, this,
      &SettingsDialog::tailscaleQueryStarted
  );
  connect(
      m_tailscaleIntegration, &deskflow::network::TailscaleIntegration::queryFinished, this,
      &SettingsDialog::tailscaleQueryFinished
  );

  loadFromConfig();
  logLevelChanged();
  createNavigation();

  setMinimumSize(760, 520);
  resize(900, 660);

  setButtonBoxEnabledButtons();
  initConnections();
}

void SettingsDialog::createNavigation()
{
  m_settingsSearch = new QLineEdit(this);
  m_settingsSearch->setObjectName(QStringLiteral("settingsSearch"));
  m_settingsSearch->setClearButtonEnabled(true);
  m_settingsSearch->setAccessibleName(tr("Search settings"));
  m_settingsSearch->setPlaceholderText(tr("Search settings · Tailscale, input, clipboard…"));
  ui->verticalLayout_3->insertWidget(0, m_settingsSearch);

  m_navigation = new QListWidget(this);
  m_navigation->setObjectName(QStringLiteral("settingsNavigation"));
  m_navigation->setAccessibleName(tr("Settings categories"));
  m_navigation->setFixedWidth(190);
  m_navigation->setWordWrap(true);
  m_navigation->setSpacing(3);
  ui->verticalLayout_3->removeWidget(ui->tabWidget);
  auto *body = new QHBoxLayout;
  body->setSpacing(16);
  body->addWidget(m_navigation);
  auto *content = new QVBoxLayout;
  m_searchEmpty = new QLabel(tr("No settings found. Try another word."), this);
  m_searchEmpty->setWordWrap(true);
  content->addWidget(m_searchEmpty);
  m_searchResults = new QListWidget(this);
  m_searchResults->setObjectName(QStringLiteral("settingsSearchResults"));
  m_searchResults->setAccessibleName(tr("Matching settings. Select a result to jump to that control."));
  m_searchResults->setMaximumHeight(172);
  m_searchResults->setWordWrap(true);
  content->addWidget(m_searchResults);
  m_pageDescription = new QLabel(this);
  m_pageDescription->setObjectName(QStringLiteral("settingsPageDescription"));
  m_pageDescription->setWordWrap(true);
  content->addWidget(m_pageDescription);
  content->addWidget(ui->tabWidget, 1);
  body->addLayout(content, 1);
  ui->verticalLayout_3->insertLayout(1, body, 1);
  ui->tabWidget->tabBar()->hide();

  // Keep the existing pages and controls, but let long pages scroll instead
  // of making every category as tall as the longest page.
  for (int i = 0; i < ui->tabWidget->count(); ++i) {
    auto *page = ui->tabWidget->widget(i);
    auto *pageContent = new QWidget;
    pageContent->setLayout(page->layout());
    auto *originalLayout = pageContent->layout();
    while (originalLayout->count() > 0 && originalLayout->itemAt(0)->spacerItem() != nullptr) {
      delete originalLayout->takeAt(0);
    }
    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(pageContent);
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 8, 0, 0);
    pageLayout->addWidget(scroll);
  }

  const QStringList pages{QStringLiteral("tabGeneral"), QStringLiteral("tabNetwork"),
                          QStringLiteral("tabInput"),   QStringLiteral("tabFileTransfer"),
                          QStringLiteral("tabWindow"),  QStringLiteral("tabAdvanced"),
                          QStringLiteral("tabLog")};
  for (const auto &name : pages) {
    auto *entry = new QListWidgetItem(m_navigation);
    entry->setData(Qt::UserRole, name);
    entry->setSizeHint(QSize(170, 48));
  }
  connect(m_navigation, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
    if (current != nullptr) {
      auto *page = ui->tabWidget->findChild<QWidget *>(current->data(Qt::UserRole).toString());
      ui->tabWidget->setCurrentWidget(page);
      const QStringList descriptions{
          tr("Language, automatic startup, and everyday app behavior."),
          tr("Choose a network, encryption, certificates, and the connection port."),
          tr("Control input language, keyboard behavior, and shared clipboard text."),
          tr("Choose where received files go and manage Pro Local activation."),
          tr("Choose what happens when you close or minimize the window."),
          tr("Service mode, screen commands, and options for managed setups."),
          tr("Choose log detail and file storage when investigating a problem.")
      };
      m_pageDescription->setText(descriptions.value(m_navigation->row(current)));
    }
  });
  connect(m_settingsSearch, &QLineEdit::textChanged, this, &SettingsDialog::filterNavigation);
  connect(m_searchResults, &QListWidget::itemActivated, this, &SettingsDialog::activateSearchResult);
  connect(m_searchResults, &QListWidget::itemClicked, this, &SettingsDialog::activateSearchResult);
  connect(m_settingsSearch, &QLineEdit::returnPressed, this, [this] {
    activateSearchResult(m_searchResults->currentItem());
  });
  auto *find = new QAction(this);
  find->setShortcut(QKeySequence::Find);
  connect(find, &QAction::triggered, m_settingsSearch, qOverload<>(&QLineEdit::setFocus));
  addAction(find);
  filterNavigation();
  m_navigation->setCurrentRow(0);
}

void SettingsDialog::selectPage(const QString &name)
{
  m_settingsSearch->clear();
  for (int i = 0; i < m_navigation->count(); ++i) {
    auto *entry = m_navigation->item(i);
    if (entry->data(Qt::UserRole).toString() == name && !entry->isHidden()) {
      m_navigation->setCurrentItem(entry);
      return;
    }
  }
}

void SettingsDialog::activateSearchResult(QListWidgetItem *result)
{
  if (result == nullptr) {
    return;
  }
  const auto pageName = result->data(Qt::UserRole + 1).toString();
  auto *page = ui->tabWidget->findChild<QWidget *>(pageName);
  auto *target = page == nullptr ? nullptr : page->findChild<QWidget *>(result->data(Qt::UserRole).toString());
  if (target == nullptr) {
    return;
  }
  for (int i = 0; i < m_navigation->count(); ++i) {
    if (m_navigation->item(i)->data(Qt::UserRole).toString() == pageName) {
      m_navigation->setCurrentItem(m_navigation->item(i));
      break;
    }
  }
  if (auto *label = qobject_cast<QLabel *>(target); label != nullptr && label->buddy() != nullptr) {
    target = label->buddy();
  }
  if (auto *scroll = page->findChild<QScrollArea *>()) {
    scroll->ensureWidgetVisible(target, 16, 48);
  }
  if (target->isEnabled() && target->focusPolicy() != Qt::NoFocus) {
    target->setFocus(Qt::ShortcutFocusReason);
  }
  m_pageDescription->setText(tr("You are here: %1").arg(result->text()));
  if (!target->isEnabled() && ui->groupTailscale->isChecked() &&
      (target == ui->sbPort || target == ui->comboInterface)) {
    m_pageDescription->setText(
        m_pageDescription->text() + QLatin1Char('\n') +
        tr("Tailscale manages this value. Turn off Tailscale in this section to change it.")
    );
  }
}

void SettingsDialog::filterNavigation()
{
  if (m_navigation == nullptr) {
    return;
  }
  const QStringList titles{tr("General"), tr("Connection & security"), tr("Keyboard & clipboard"), tr("File transfer"),
                           tr("Window"),  tr("System & automation"),   tr("Logs & diagnostics")};
  const auto words = m_settingsSearch->text().simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  m_searchResults->clear();
  QListWidgetItem *firstMatch = nullptr;
  for (int i = 0; i < m_navigation->count(); ++i) {
    auto *entry = m_navigation->item(i);
    entry->setText(titles[i]);
    auto *page = ui->tabWidget->findChild<QWidget *>(entry->data(Qt::UserRole).toString());
    QString searchable = titles[i];
    for (auto *control : page->findChildren<QWidget *>()) {
      searchable += QLatin1Char(' ') + control->property("text").toString() + QLatin1Char(' ') +
                    control->property("title").toString() + QLatin1Char(' ') + control->toolTip();
      if (auto *combo = qobject_cast<QComboBox *>(control)) {
        for (int option = 0; option < combo->count(); ++option) {
          searchable += QLatin1Char(' ') + combo->itemText(option);
        }
      }
      if (!words.isEmpty() && ui->tabWidget->isTabVisible(ui->tabWidget->indexOf(page)) && !control->isHidden() &&
          !control->objectName().isEmpty() && control != ui->lblTailscaleStatus) {
        auto label = control->property("text").toString();
        if (label.isEmpty()) {
          label = control->property("title").toString();
        }
        if (label.isEmpty()) {
          continue;
        }
        label.remove(QLatin1Char('&'));
        label.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
        const auto terms = titles[i] + QLatin1Char(' ') + label + QLatin1Char(' ') + control->toolTip();
        bool matchesControl = true;
        for (const auto &word : words) {
          matchesControl = matchesControl && terms.contains(word, Qt::CaseInsensitive);
        }
        if (matchesControl) {
          auto *result =
              new QListWidgetItem(titles[i] + QStringLiteral(" › ") + label.simplified().left(150), m_searchResults);
          result->setData(Qt::UserRole, control->objectName());
          result->setData(Qt::UserRole + 1, entry->data(Qt::UserRole));
          result->setToolTip(control->toolTip());
        }
      }
    }
    bool matches = ui->tabWidget->isTabVisible(ui->tabWidget->indexOf(page));
    for (const auto &word : words) {
      matches = matches && searchable.contains(word, Qt::CaseInsensitive);
    }
    entry->setHidden(!matches);
    if (matches && firstMatch == nullptr) {
      firstMatch = entry;
    }
  }
  m_searchEmpty->setVisible(firstMatch == nullptr);
  m_searchResults->setVisible(!words.isEmpty() && m_searchResults->count() > 0);
  int resultsHeight = 4;
  for (int i = 0; i < qMin(5, m_searchResults->count()); ++i) {
    resultsHeight += m_searchResults->sizeHintForRow(i);
  }
  m_searchResults->setFixedHeight(qBound(48, resultsHeight, 172));
  m_searchResults->setCurrentRow(0);
  ui->tabWidget->setVisible(firstMatch != nullptr);
  if (firstMatch != nullptr && (m_navigation->currentItem() == nullptr || m_navigation->currentItem()->isHidden())) {
    m_navigation->setCurrentItem(firstMatch);
  }
}

void SettingsDialog::changeEvent(QEvent *e)
{
  QDialog::changeEvent(e);
  if (e->type() == QEvent::LanguageChange) {
    ui->retranslateUi(this);
    updateText();
    updateFileTransferControls();
    filterNavigation();
    if (m_settingsSearch != nullptr) {
      m_settingsSearch->setPlaceholderText(tr("Search settings · Tailscale, input, clipboard…"));
    }
  } else if (e->type() == QEvent::PaletteChange) {
    applyIeumDialogStyle(*this);
  }
}

void SettingsDialog::initConnections() const
{
  connect(this, &SettingsDialog::shown, this, &SettingsDialog::showReadOnlyMessage, Qt::QueuedConnection);

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(ui->buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &SettingsDialog::loadFromConfig);
  connect(
      ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
      &SettingsDialog::resetToDefault
  );

  connect(ui->cbRunEnterCommand, &QCheckBox::toggled, ui->lineCommandEnter, &QLineEdit::setEnabled);
  connect(ui->cbRunExitCommand, &QCheckBox::toggled, ui->lineCommandExit, &QLineEdit::setEnabled);

  connect(ui->groupSecurity, &QGroupBox::toggled, this, &SettingsDialog::updateTlsControlsEnabled);
  connect(ui->groupService, &QGroupBox::toggled, this, &SettingsDialog::updateControls);
  connect(ui->btnTlsRegenCert, &QPushButton::clicked, this, &SettingsDialog::regenCertificates);
  connect(ui->comboTlsKeyLength, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateRequestedKeySize);
  connect(ui->btnTlsCertPath, &QPushButton::clicked, this, &SettingsDialog::browseCertificatePath);
  connect(ui->btnBrowseLog, &QPushButton::clicked, this, &SettingsDialog::browseLogPath);
  connect(ui->btnBrowseFileTransfer, &QPushButton::clicked, this, &SettingsDialog::browseFileTransferDirectory);
  connect(ui->btnGetProEarlyAccess, &QPushButton::clicked, this, &SettingsDialog::openProEarlyAccessSponsor);
  connect(ui->btnCopyProEarlyAccessClaim, &QPushButton::clicked, this, &SettingsDialog::copyProEarlyAccessClaim);
  connect(ui->btnEmailProLicenseRequest, &QPushButton::clicked, this, &SettingsDialog::emailProLicenseRequest);
  connect(ui->btnImportProLicense, &QPushButton::clicked, this, &SettingsDialog::importProLicense);
  connect(ui->btnActivateProLicense, &QPushButton::clicked, this, &SettingsDialog::activateProLicense);
  connect(ui->btnRemoveProLicense, &QPushButton::clicked, this, &SettingsDialog::removeProLicense);
  connect(ui->groupLogToFile, &QGroupBox::toggled, this, &SettingsDialog::setLogToFile);
  connect(ui->comboLogLevel, &QComboBox::currentIndexChanged, this, &SettingsDialog::logLevelChanged);
  connect(ui->comboLanguage, &QComboBox::currentTextChanged, this, [](const QString &lang) {
    const auto shortName = I18N::nativeTo639Name(lang);
    I18N::setLanguage(shortName);
  });

  // Connect modifiable controls
  connect(ui->rbIconMono, &QRadioButton::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->sbPort, &QSpinBox::valueChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->comboLogLevel, &QComboBox::currentIndexChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->comboInterface, &QComboBox::currentIndexChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->comboInterface, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateControls);
  connect(ui->cbPreferPhysicalNetwork, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->groupTailscale, &QGroupBox::toggled, this, &SettingsDialog::tailscaleToggled);
  connect(ui->btnRefreshTailscale, &QPushButton::clicked, this, &SettingsDialog::updateTailscaleStatus);
  connect(ui->comboTlsKeyLength, &QComboBox::currentIndexChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->comboLanguage, &QComboBox::currentIndexChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->rbAutoHide, &QRadioButton::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbPreventSleep, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->rbCloseToTray, &QRadioButton::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbElevateDaemon, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbAutoUpdate, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbStartAtLogin, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbGuiDebug, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbShowVersion, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbRequireClientCert, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->groupLogToFile, &QGroupBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->groupService, &QGroupBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->groupSecurity, &QGroupBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineLogFilename, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineTlsCertPath, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbRunEnterCommand, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbRunExitCommand, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineCommandEnter, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineCommandExit, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbImeSync, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->comboCjkRawScancode, &QComboBox::currentIndexChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->comboEnterScreenLang, &QComboBox::currentIndexChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbClipboardNormalizeNfc, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->spinMacInterKeyDelayMicros, &QSpinBox::valueChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbFileTransferEnabled, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbFileTransferReceive, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbFileTransferReceive, &QCheckBox::toggled, this, &SettingsDialog::updateFileTransferControls);
  connect(ui->lineFileTransferDirectory, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineProLicense, &QLineEdit::textChanged, this, &SettingsDialog::updateFileTransferControls);
}

void SettingsDialog::tailscaleToggled(bool enabled)
{
  if (m_loadingConfig) {
    return;
  }

  if (enabled) {
    if (!m_previousNetworkCaptured) {
      m_previousInterface = ui->comboInterface->currentData().toString();
      m_previousPreferPhysical = ui->cbPreferPhysicalNetwork->isChecked();
      m_previousPort = ui->sbPort->value();
      m_previousNetworkCaptured = true;
    }
    updateTailscaleStatus();
  } else {
    m_acceptAfterTailscaleCheck = false;
    m_tailscaleIntegration->cancel();
    m_tailscaleQueryInProgress = false;
    m_tailscaleStatusChecked = false;
    if (m_previousNetworkCaptured) {
      if (!m_previousInterface.isEmpty() && ui->comboInterface->findData(m_previousInterface) < 0) {
        ui->comboInterface->addItem(m_previousInterface, m_previousInterface);
      }
      const auto interfaceIndex = m_previousInterface.isEmpty() ? 0 : ui->comboInterface->findData(m_previousInterface);
      ui->comboInterface->setCurrentIndex(interfaceIndex < 0 ? 0 : interfaceIndex);
      ui->cbPreferPhysicalNetwork->setChecked(m_previousPreferPhysical);
      ui->sbPort->setValue(m_previousPort);
    }
    ui->lblTailscaleStatus->setText(tailscaleStatusText());
    ui->lblTailscaleStatus->setToolTip(QString());
  }

  updateControls();
  setButtonBoxEnabledButtons();
}

void SettingsDialog::updateTailscaleStatus()
{
  m_tailscaleIntegration->query();
}

void SettingsDialog::tailscaleQueryStarted()
{
  m_tailscaleQueryInProgress = true;
  m_tailscaleStatusChecked = false;
  ui->lblTailscaleStatus->setText(tr("Checking Tailscale..."));
  ui->lblTailscaleStatus->setToolTip(QString());
  updateControls();
  setButtonBoxEnabledButtons();
}

void SettingsDialog::tailscaleQueryFinished(const deskflow::network::TailscaleStatus &status)
{
  m_tailscaleStatus = status;
  m_tailscaleQueryInProgress = false;
  m_tailscaleStatusChecked = true;
  ui->lblTailscaleStatus->setText(tailscaleStatusText());
  ui->lblTailscaleStatus->setToolTip(m_tailscaleStatus.error);

  if (ui->groupTailscale->isChecked() && m_tailscaleStatus.isReady()) {
    applyTailscalePreset();
  }
  updateControls();
  setButtonBoxEnabledButtons();

  if (m_acceptAfterTailscaleCheck) {
    m_acceptAfterTailscaleCheck = false;
    acceptWithTailscaleStatus();
  }
}

void SettingsDialog::applyTailscalePreset()
{
  const auto address = m_tailscaleStatus.preferredLocalAddress();
  if (address.isEmpty()) {
    return;
  }

  if (ui->comboInterface->findData(address) < 0) {
    ui->comboInterface->addItem(address, address);
  }
  ui->comboInterface->setCurrentIndex(ui->comboInterface->findData(address));
  ui->cbPreferPhysicalNetwork->setChecked(false);
  ui->sbPort->setValue(Settings::defaultValue(Settings::Core::Port).toInt());
}

QString SettingsDialog::tailscaleStatusText() const
{
  if (!m_tailscaleStatusChecked) {
    return tr("Turn on to check Tailscale");
  }

  using deskflow::network::TailscaleState;
  switch (m_tailscaleStatus.state) {
  case TailscaleState::Running:
    if (m_tailscaleStatus.isReady()) {
      return tr("Ready | This device: %1 | %n online computer(s)", "", m_tailscaleStatus.onlineDesktopPeers().size())
          .arg(m_tailscaleStatus.preferredLocalAddress());
    }
    return tr("Tailscale has no usable address");
  case TailscaleState::NotInstalled:
    return tr("Tailscale was not found");
  case TailscaleState::Stopped:
    return tr("Tailscale is not running");
  case TailscaleState::NeedsLogin:
    return tr("Sign in to Tailscale");
  case TailscaleState::Error:
    return tr("Could not read Tailscale status");
  }
  return tr("Could not read Tailscale status");
}

void SettingsDialog::regenCertificates()
{
  if (TlsUtility::generateCertificate()) {
    QMessageBox::information(this, tr("TLS Certificate Regenerated"), tr("TLS certificate regenerated successfully."));
    const auto certificate = Settings::value(Settings::Security::Certificate).toString();
    updateKeyLengthOnFile(certificate);
  }
}

void SettingsDialog::browseCertificatePath()
{
  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Select a TLS certificate to use..."), ui->lineTlsCertPath->text(), "Cert (*.pem)", nullptr,
      QFileDialog::DontConfirmOverwrite
  );

  if (!fileName.isEmpty()) {
    ui->lineTlsCertPath->setText(fileName);

    if (QFile(fileName).exists()) {
      updateKeyLengthOnFile(fileName);
    } else {
      qDebug("no tls certificate file at: %s", qUtf8Printable(fileName));
    }
  }
}

void SettingsDialog::browseLogPath()
{
  QString fileName =
      QFileDialog::getSaveFileName(this, tr("Save log file to..."), ui->lineLogFilename->text(), "Logs (*.log *.txt)");

  if (!fileName.isEmpty()) {
    ui->lineLogFilename->setText(fileName);
  }
}

void SettingsDialog::browseFileTransferDirectory()
{
  const auto directory = QFileDialog::getExistingDirectory(
      this, tr("Choose where received files are saved"), ui->lineFileTransferDirectory->text()
  );
  if (!directory.isEmpty()) {
    ui->lineFileTransferDirectory->setText(QDir::cleanPath(directory));
  }
}

void SettingsDialog::ensureProEarlyAccessClaim()
{
  if (!deskflow::licensing::isValidProEarlyAccessClaim(m_proEarlyAccessClaim)) {
    m_proEarlyAccessClaim = deskflow::licensing::createProEarlyAccessClaim();
  }
  ui->lineProEarlyAccessClaim->setText(m_proEarlyAccessClaim);
}

void SettingsDialog::openProEarlyAccessSponsor()
{
  ensureProEarlyAccessClaim();
  const auto url = deskflow::licensing::proEarlyAccessSponsorUrl(m_proEarlyAccessClaim);
  if (!url.isEmpty()) {
    deskflow::gui::openExternalUrlOrShowFallback(this, url, tr("Open Pro Local Sponsor Page"));
  }
}

void SettingsDialog::copyProEarlyAccessClaim()
{
  ensureProEarlyAccessClaim();
  if (deskflow::licensing::isValidProEarlyAccessClaim(m_proEarlyAccessClaim)) {
    QGuiApplication::clipboard()->setText(m_proEarlyAccessClaim);
  }
}

void SettingsDialog::emailProLicenseRequest()
{
  ensureProEarlyAccessClaim();
  const auto url = deskflow::licensing::proEarlyAccessMailtoUrl(m_proEarlyAccessClaim);
  if (!url.isEmpty()) {
    (void)QDesktopServices::openUrl(url);
  }
}

void SettingsDialog::importProLicense()
{
  const auto fileName = QFileDialog::getOpenFileName(
      this, tr("Import Pro Local license"), QString(), tr("Ieum license (*.ieum-license.txt *.txt);;All files (*)")
  );
  if (fileName.isEmpty()) {
    return;
  }

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, tr("Could not import license"), tr("Ieum could not read the selected license file."));
    return;
  }

  const auto bytes = file.read(deskflow::licensing::kMaxProLicenseBytes + 1);
  if (bytes.size() > deskflow::licensing::kMaxProLicenseBytes) {
    QMessageBox::warning(this, tr("Could not import license"), tr("The selected license file is too large."));
    return;
  }

  QStringDecoder decoder(QStringDecoder::Utf8);
  const QString decoded = decoder.decode(bytes);
  const auto candidate = decoded.trimmed();
  if (decoder.hasError() || candidate.contains(QLatin1Char('\n')) || candidate.contains(QLatin1Char('\r'))) {
    QMessageBox::warning(
        this, tr("Could not import license"), tr("The license file must contain exactly one UTF-8 license line.")
    );
    return;
  }

  ui->lineProLicense->setText(candidate);
  ui->lineProLicense->setFocus();
}

void SettingsDialog::activateProLicense()
{
  if (!Settings::isWritable() || fileTransferPlatformSupport() == FileTransferPlatformSupport::Unsupported) {
    return;
  }

  const auto candidate = ui->lineProLicense->text().trimmed();
  const auto entitlement = deskflow::licensing::verifyProductionProLicense(candidate);
  if (!shouldActivateProFileTransferLicense(entitlement)) {
    QMessageBox::warning(
        this, tr("License not activated"),
        tr("This license is invalid, expired, not active yet, or does not include Pro Local file transfer.")
    );
    updateFileTransferControls();
    return;
  }

  Settings::setValue(Settings::Pro::LicenseKey, candidate);
  ui->lineProLicense->setText(candidate);
  updateFileTransferControls();
  QMessageBox::information(this, tr("Pro Local activated"), tr("File transfer is now available on this computer."));
}

void SettingsDialog::removeProLicense()
{
  if (!Settings::isWritable() || fileTransferPlatformSupport() == FileTransferPlatformSupport::Unsupported) {
    return;
  }

  Settings::setValue(Settings::Pro::LicenseKey, QString{});
  Settings::setValue(Settings::FileTransfer::Enabled, false);
  Settings::setValue(Settings::FileTransfer::ReceiveEnabled, false);
  ui->lineProLicense->clear();
  ui->cbFileTransferEnabled->setChecked(false);
  ui->cbFileTransferReceive->setChecked(false);
  updateFileTransferControls();
  setButtonBoxEnabledButtons();
}

void SettingsDialog::setLogToFile(bool logToFile)
{
  ui->widgetLogFilename->setEnabled(logToFile);
}

void SettingsDialog::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);
  Q_EMIT shown();
}

void SettingsDialog::showReadOnlyMessage()
{
  if (Settings::isWritable())
    return;
  messages::showReadOnlySettings(this, Settings::settingsFile());
}

void SettingsDialog::updateText()
{
  const auto logLevels = LogLevel::logLevelNames().count();
  const QStringList toolTips = {tr("Required messages"),        tr("Non-fatal errors"), tr("General warnings"),
                                tr("General events [Default]"), tr("Debug entries"),    tr("Verbose debug output")};
  if (ui->comboLogLevel->count() == 0) {
    const auto logLevelOptions = LogLevel::logLevelOptions();
    for (int i = 0; i < logLevels; i++) {
      ui->comboLogLevel->addItem(LogLevel::toString(i));
      ui->comboLogLevel->setItemData(i, logLevelOptions.at(i), Qt::UserRole);
      ui->comboLogLevel->setItemData(i, toolTips.at(i), Qt::ToolTipRole);
    }
  } else {
    for (int i = 0; i < logLevels; i++) {
      ui->comboLogLevel->setItemData(i, LogLevel::toString(i), Qt::DisplayRole);
      ui->comboLogLevel->setItemData(i, toolTips.at(i), Qt::ToolTipRole);
    }
  }
  ui->buttonBox->button(QDialogButtonBox::Save)->setToolTip(tr("Close and save changes"));
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setToolTip(tr("Close and forget changes"));
  ui->buttonBox->button(QDialogButtonBox::Reset)->setToolTip(tr("Reset to stored values"));
  ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setToolTip(tr("Reset to default values"));
  ui->comboCjkRawScancode->setItemText(0, tr("Automatic"));
  ui->comboCjkRawScancode->setItemText(1, tr("Always"));
  ui->comboCjkRawScancode->setItemText(2, tr("Never"));
  ui->comboEnterScreenLang->setItemText(0, tr("Keep remote state"));
  ui->comboEnterScreenLang->setItemText(1, tr("Force English"));
  ui->comboEnterScreenLang->setItemText(2, tr("Follow this computer"));
  ui->lblTailscaleStatus->setText(tailscaleStatusText());
}

void SettingsDialog::accept()
{
  if (ui->groupTailscale->isChecked()) {
    m_acceptAfterTailscaleCheck = true;
    updateTailscaleStatus();
    return;
  }

  acceptWithTailscaleStatus();
}

void SettingsDialog::acceptWithTailscaleStatus()
{
  bool startupApprovalRequired = false;
  const auto canSaveFileTransfer = Settings::hasProFileTransferEntitlement() && ui->groupSecurity->isChecked();

  if (canSaveFileTransfer && ui->cbFileTransferReceive->isChecked() &&
      !QDir::isAbsolutePath(ui->lineFileTransferDirectory->text().trimmed())) {
    QMessageBox::warning(
        this, tr("Choose a destination folder"),
        tr("Select an absolute folder before enabling incoming file transfers.")
    );
    return;
  }

  if (ui->groupTailscale->isChecked()) {
    if (!m_tailscaleStatus.isReady()) {
      QMessageBox::warning(
          this, tr("Tailscale is not ready"),
          tr("Start Tailscale and sign in, then refresh its status before saving this setting.")
      );
      return;
    }
    applyTailscalePreset();
  }

  if (StartupManager::isSupported()) {
    QString startupError;
    if (!StartupManager::setEnabled(ui->cbStartAtLogin->isChecked(), &startupError)) {
      QMessageBox::warning(
          this, tr("Could not update automatic startup"),
          tr("Ieum could not update its automatic startup setting.\n\n%1").arg(startupError)
      );
      return;
    }
    startupApprovalRequired = ui->cbStartAtLogin->isChecked() && StartupManager::requiresApproval();
  }

  const auto tailscaleWasEnabled = Settings::value(Settings::Core::UseTailscale).toBool();
  if (!tailscaleWasEnabled && ui->groupTailscale->isChecked()) {
    Settings::setValue(Settings::Core::TailscalePreviousInterface, m_previousInterface);
    Settings::setValue(Settings::Core::TailscalePreviousPreferPhysical, m_previousPreferPhysical);
    Settings::setValue(Settings::Core::TailscalePreviousPort, m_previousPort);
    Settings::setValue(Settings::Client::TailscalePreviousRemoteHost, Settings::value(Settings::Client::RemoteHost));
    Settings::setValue(Settings::Client::RemoteHost);
  } else if (tailscaleWasEnabled && !ui->groupTailscale->isChecked()) {
    Settings::setValue(Settings::Client::RemoteHost, Settings::value(Settings::Client::TailscalePreviousRemoteHost));
  }

  Settings::setValue(Settings::Core::Port, ui->sbPort->value());
  Settings::setValue(Settings::Core::Interface, ui->comboInterface->currentData());
  Settings::setValue(Settings::Core::PreferPhysicalNetwork, ui->cbPreferPhysicalNetwork->isChecked());
  Settings::setValue(Settings::Core::UseTailscale, ui->groupTailscale->isChecked());
  Settings::setValue(Settings::Log::Level, ui->comboLogLevel->currentData());
  Settings::setValue(Settings::Log::ToFile, ui->groupLogToFile->isChecked());
  Settings::setValue(Settings::Log::File, ui->lineLogFilename->text());
  Settings::setValue(Settings::Daemon::Elevate, ui->cbElevateDaemon->isChecked());
  Settings::setValue(Settings::Gui::Autohide, ui->rbAutoHide->isChecked());
  Settings::setValue(Settings::Gui::StartAtLogin, ui->cbStartAtLogin->isChecked());
  Settings::setValue(Settings::Gui::AutoUpdateCheck, ui->cbAutoUpdate->isChecked());
  Settings::setValue(Settings::Core::PreventSleep, ui->cbPreventSleep->isChecked());
  Settings::setValue(Settings::Security::Certificate, ui->lineTlsCertPath->text());
  Settings::setValue(Settings::Security::KeySize, ui->comboTlsKeyLength->currentText().toInt());
  Settings::setValue(Settings::Security::TlsEnabled, ui->groupSecurity->isChecked());
  Settings::setValue(Settings::Gui::CloseToTray, ui->rbCloseToTray->isChecked());
  Settings::setValue(Settings::Gui::SymbolicTrayIcon, ui->rbIconMono->isChecked());
  Settings::setValue(Settings::Security::CheckPeers, ui->cbRequireClientCert->isChecked());
  Settings::setValue(Settings::Core::Language, I18N::nativeTo639Name(ui->comboLanguage->currentText()));
  Settings::setValue(Settings::Log::GuiDebug, ui->cbGuiDebug->isChecked());
  Settings::setValue(Settings::Gui::ShowVersionInTitle, ui->cbShowVersion->isChecked());
  Settings::setValue(Settings::Core::EnableEnterCommand, ui->cbRunEnterCommand->isChecked());
  Settings::setValue(Settings::Core::EnableExitCommand, ui->cbRunExitCommand->isChecked());
  Settings::setValue(Settings::Core::ScreenEnterCommand, ui->lineCommandEnter->text());
  Settings::setValue(Settings::Core::ScreenExitCommand, ui->lineCommandExit->text());
  Settings::setValue(Settings::Core::ImeSync, ui->cbImeSync->isChecked());
  Settings::setValue(Settings::Client::CjkRawScancode, ui->comboCjkRawScancode->currentData());
  Settings::setValue(Settings::Core::EnterScreenLang, ui->comboEnterScreenLang->currentData());
  Settings::setValue(Settings::Client::ClipboardNormalizeNfc, ui->cbClipboardNormalizeNfc->isChecked());
  Settings::setValue(Settings::Client::MacInterKeyDelayMicros, ui->spinMacInterKeyDelayMicros->value());
  Settings::setValue(Settings::FileTransfer::Enabled, canSaveFileTransfer && ui->cbFileTransferEnabled->isChecked());
  Settings::setValue(
      Settings::FileTransfer::ReceiveEnabled, canSaveFileTransfer && ui->cbFileTransferReceive->isChecked()
  );
  Settings::setValue(
      Settings::FileTransfer::DownloadDirectory, QDir::cleanPath(ui->lineFileTransferDirectory->text().trimmed())
  );

  Settings::ProcessMode mode;
  if (ui->groupService->isChecked())
    mode = Settings::ProcessMode::Service;
  else
    mode = Settings::ProcessMode::Desktop;
  Settings::setValue(Settings::Core::ProcessMode, mode);

  if (startupApprovalRequired) {
    const auto choice = QMessageBox::warning(
        this, tr("Allow Ieum at login"),
        tr("macOS still requires approval for automatic startup. Open System Settings > General > Login Items "
           "and allow Ieum."),
        QMessageBox::Open | QMessageBox::Cancel, QMessageBox::Open
    );
    if (choice == QMessageBox::Open) {
      StartupManager::openSystemSettings();
    }
  }

  QDialog::accept();
}

void SettingsDialog::loadFromConfig()
{
  m_acceptAfterTailscaleCheck = false;
  m_tailscaleIntegration->cancel();
  m_tailscaleQueryInProgress = false;
  m_loadingConfig = true;
  ui->sbPort->setValue(Settings::value(Settings::Core::Port).toInt());
  ui->comboLogLevel->setCurrentIndex(
      ui->comboLogLevel->findData(Settings::logLevelText(), Qt::UserRole, Qt::MatchFixedString)
  );
  ui->groupLogToFile->setChecked(Settings::value(Settings::Log::ToFile).toBool());
  ui->lineLogFilename->setText(Settings::value(Settings::Log::File).toString());
  ui->cbPreventSleep->setChecked(Settings::value(Settings::Core::PreventSleep).toBool());
  ui->cbPreferPhysicalNetwork->setChecked(Settings::value(Settings::Core::PreferPhysicalNetwork).toBool());
  const auto tailscaleEnabled = Settings::value(Settings::Core::UseTailscale).toBool();
  ui->groupTailscale->setChecked(tailscaleEnabled);
  if (tailscaleEnabled) {
    m_previousInterface = Settings::value(Settings::Core::TailscalePreviousInterface).toString();
    m_previousPreferPhysical = Settings::value(Settings::Core::TailscalePreviousPreferPhysical).toBool();
    m_previousPort = Settings::value(Settings::Core::TailscalePreviousPort).toInt();
    m_previousNetworkCaptured = true;
  } else {
    m_previousInterface = Settings::value(Settings::Core::Interface).toString();
    m_previousPreferPhysical = Settings::value(Settings::Core::PreferPhysicalNetwork).toBool();
    m_previousPort = Settings::value(Settings::Core::Port).toInt();
    m_previousNetworkCaptured = false;
  }
  ui->cbElevateDaemon->setChecked(Settings::value(Settings::Daemon::Elevate).toBool());
  ui->cbStartAtLogin->setChecked(Settings::value(Settings::Gui::StartAtLogin).toBool());
  ui->cbStartAtLogin->setVisible(StartupManager::isSupported());
  ui->cbAutoUpdate->setChecked(Settings::value(Settings::Gui::AutoUpdateCheck).toBool());
  ui->cbGuiDebug->setChecked(Settings::value(Settings::Log::GuiDebug).toBool());
  ui->cbShowVersion->setChecked(Settings::value(Settings::Gui::ShowVersionInTitle).toBool());
  ui->cbRunEnterCommand->setChecked(Settings::value(Settings::Core::EnableEnterCommand).toBool());
  ui->cbRunExitCommand->setChecked(Settings::value(Settings::Core::EnableExitCommand).toBool());
  ui->lineCommandEnter->setText(Settings::value(Settings::Core::ScreenEnterCommand).toString());
  ui->lineCommandExit->setText(Settings::value(Settings::Core::ScreenExitCommand).toString());
  ui->cbImeSync->setChecked(Settings::value(Settings::Core::ImeSync).toBool());
  selectComboData(
      ui->comboCjkRawScancode, Settings::value(Settings::Client::CjkRawScancode).toString(),
      Settings::defaultValue(Settings::Client::CjkRawScancode).toString()
  );
  selectComboData(
      ui->comboEnterScreenLang, Settings::value(Settings::Core::EnterScreenLang).toString(),
      Settings::defaultValue(Settings::Core::EnterScreenLang).toString()
  );
  ui->cbClipboardNormalizeNfc->setChecked(Settings::value(Settings::Client::ClipboardNormalizeNfc).toBool());
  ui->spinMacInterKeyDelayMicros->setValue(Settings::value(Settings::Client::MacInterKeyDelayMicros).toInt());
  ui->cbFileTransferEnabled->setChecked(Settings::value(Settings::FileTransfer::Enabled).toBool());
  ui->cbFileTransferReceive->setChecked(Settings::value(Settings::FileTransfer::ReceiveEnabled).toBool());
  ui->lineFileTransferDirectory->setText(Settings::value(Settings::FileTransfer::DownloadDirectory).toString());
  ui->lineProLicense->setText(Settings::value(Settings::Pro::LicenseKey).toString());

  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  ui->groupService->setChecked(processMode == Settings::ProcessMode::Service);

  if (!deskflow::platform::isWindows())
    ui->groupService->setVisible(false);
  ui->tabWidget->setTabVisible(
      ui->tabWidget->indexOf(ui->tabFileTransfer),
      fileTransferPlatformSupport() != FileTransferPlatformSupport::Unsupported
  );

  if (Settings::value(Settings::Gui::SymbolicTrayIcon).toBool())
    ui->rbIconMono->setChecked(true);
  else
    ui->rbIconColorful->setChecked(true);

  const auto autoHide = Settings::value(Settings::Gui::Autohide).toBool();
  ui->rbAutoHide->setChecked(autoHide);
  ui->rbShowOnStart->setChecked(!autoHide);

  const auto closeToTray = Settings::value(Settings::Gui::CloseToTray).toBool();
  ui->rbCloseToTray->setChecked(closeToTray);
  ui->rbExitOnClose->setChecked(!closeToTray);

  ui->lblDebugWarning->setVisible(
      Settings::value(Settings::Log::Level).toInt() > static_cast<int>(LogLevel::Level::Info)
  );

  const auto configuredInterface = Settings::value(Settings::Core::Interface).toString();
  const auto interfaceIndex = configuredInterface.isEmpty() ? 0 : ui->comboInterface->findData(configuredInterface);
  ui->comboInterface->setCurrentIndex(interfaceIndex < 0 ? 0 : interfaceIndex);

  m_loadingConfig = false;
  if (tailscaleEnabled) {
    updateTailscaleStatus();
  } else {
    m_tailscaleStatusChecked = false;
    ui->lblTailscaleStatus->setText(tailscaleStatusText());
    ui->lblTailscaleStatus->setToolTip(QString());
  }
  qDebug() << "load from config done";

  updateControls();
}

void SettingsDialog::updateTlsControls()
{
  const auto certificate = Settings::value(Settings::Security::Certificate).toString();
  if (QFile(certificate).exists()) {
    updateKeyLengthOnFile(certificate);
  }

  ui->comboTlsKeyLength->setCurrentText(Settings::value(Settings::Security::KeySize).toString());

  ui->lineTlsCertPath->setText(certificate);
  ui->cbRequireClientCert->setChecked(Settings::value(Settings::Security::CheckPeers).toBool());
  ui->groupSecurity->setChecked(TlsUtility::isEnabled());

  ui->groupSecurity->setEnabled(Settings::isWritable());

  updateTlsControlsEnabled();
}

void SettingsDialog::updateTlsControlsEnabled()
{
  const auto writable = Settings::isWritable();
  const auto tlsChecked = ui->groupSecurity->isChecked();

  auto enabled = writable && tlsChecked;
  ui->lblTlsKeyLength->setEnabled(enabled);
  ui->comboTlsKeyLength->setEnabled(enabled);
  ui->lblTlsCert->setEnabled(enabled);
  ui->widgetTlsCert->setEnabled(enabled);
  ui->btnTlsRegenCert->setEnabled(enabled);
  ui->cbRequireClientCert->setEnabled(enabled && !isClientMode());
  updateFileTransferControls();
}

void SettingsDialog::updateFileTransferControls()
{
  const auto storedLicense = Settings::value(Settings::Pro::LicenseKey).toString().trimmed();
  const auto storedEntitlement = deskflow::licensing::verifyProductionProLicense(storedLicense);
  const auto state = proFileTransferUiState(
      storedEntitlement, !storedLicense.isEmpty(), Settings::isWritable(), fileTransferPlatformSupport(),
      ui->groupSecurity->isChecked()
  );

  const auto candidate = ui->lineProLicense->text().trimmed();
  const auto candidateEntitlement = deskflow::licensing::verifyProductionProLicense(candidate);
  const auto storedLicenseActive = shouldActivateProFileTransferLicense(storedEntitlement);
  const auto candidateCanActivate = shouldActivateProFileTransferLicense(candidateEntitlement);
  const auto candidateIsStored = candidate == storedLicense;

  ui->lineProLicense->setEnabled(state.licenseActionsEnabled);
  ui->btnImportProLicense->setEnabled(state.licenseActionsEnabled);
  ui->btnActivateProLicense->setEnabled(state.licenseActionsEnabled && candidateCanActivate && !candidateIsStored);
  ui->btnRemoveProLicense->setEnabled(state.removeLicenseEnabled);
  ensureProEarlyAccessClaim();
  ui->widgetProEarlyAccess->setVisible(state.purchaseActionsVisible);
  const auto claimAvailable = deskflow::licensing::isValidProEarlyAccessClaim(m_proEarlyAccessClaim);
  ui->btnGetProEarlyAccess->setEnabled(claimAvailable);
  ui->btnCopyProEarlyAccessClaim->setEnabled(claimAvailable);
  ui->btnEmailProLicenseRequest->setEnabled(claimAvailable);

  QString licenseStatus;
  if (candidateIsStored && storedLicenseActive) {
    licenseStatus = tr("Pro Local active — %1 (%2)").arg(storedEntitlement.licenseId, storedEntitlement.recipient);
  } else if (candidateCanActivate) {
    licenseStatus = tr("Valid Pro Local license. Select Activate to apply it on this computer.");
  } else if (!candidateIsStored && storedLicenseActive) {
    licenseStatus = tr("This candidate is not valid. The existing Pro Local license remains active.");
  } else {
    switch (candidateEntitlement.status) {
    case deskflow::licensing::ProLicenseStatus::Missing:
      licenseStatus = tr("Pro Local is locked. Import a license file, then select Activate.");
      break;
    case deskflow::licensing::ProLicenseStatus::TooLarge:
      licenseStatus = tr("This license is too large.");
      break;
    case deskflow::licensing::ProLicenseStatus::Malformed:
      licenseStatus = tr("This license has an invalid format.");
      break;
    case deskflow::licensing::ProLicenseStatus::Unsupported:
      licenseStatus = tr("This license is not supported by this version of Ieum.");
      break;
    case deskflow::licensing::ProLicenseStatus::InvalidSignature:
      licenseStatus = tr("This license signature is invalid.");
      break;
    case deskflow::licensing::ProLicenseStatus::NotYetValid:
      licenseStatus = tr("This license is not active yet.");
      break;
    case deskflow::licensing::ProLicenseStatus::Expired:
      licenseStatus = tr("This license has expired.");
      break;
    case deskflow::licensing::ProLicenseStatus::Valid:
      licenseStatus = tr("This license does not include Pro Local file transfer.");
      break;
    }
  }
  ui->lblProLicenseStatus->setText(licenseStatus);

  ui->cbFileTransferEnabled->setEnabled(state.transferControlsEnabled);
  ui->cbFileTransferReceive->setEnabled(state.transferControlsEnabled);
  ui->widgetFileTransferDestination->setEnabled(
      state.transferControlsEnabled && ui->cbFileTransferReceive->isChecked()
  );
  ui->lblFileTransferTls->setEnabled(ui->groupSecurity->isChecked());
}

bool SettingsDialog::isClientMode() const
{
  return Settings::value(Settings::Core::CoreMode) == Settings::CoreMode::Client;
}

void SettingsDialog::updateKeyLengthOnFile(const QString &path)
{
  if (!QFile(path).exists()) {
    qFatal("tls certificate file not found: %s", qUtf8Printable(path));
  }

  auto length = TlsUtility::getCertKeyLength(path);
  auto labelIcon = QPixmap(QIcon::fromTheme(QIcon::ThemeIcon::SecurityLow).pixmap(24, 24));
  if (length == 2048)
    labelIcon = QPixmap(QIcon::fromTheme(QStringLiteral("security-medium")).pixmap(24, 24));
  if (length == 4096)
    labelIcon = QPixmap(QIcon::fromTheme(QIcon::ThemeIcon::SecurityHigh).pixmap(24, 24));

  ui->lblTlsCertInfo->setPixmap(labelIcon);
  ui->lblTlsCertInfo->setToolTip(QStringLiteral("Key length: %1 bits").arg(QString::number(length)));
}

void SettingsDialog::updateControls()
{
  const bool writable = Settings::isWritable();
  const bool serviceChecked = ui->groupService->isChecked();
  const bool logToFile = ui->groupLogToFile->isChecked();
  const bool tailscaleEnabled = ui->groupTailscale->isChecked();

  ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(writable && !m_tailscaleQueryInProgress);

  ui->groupTailscale->setEnabled(writable);
  ui->btnRefreshTailscale->setEnabled(writable && !m_tailscaleQueryInProgress);
  ui->sbPort->setEnabled(writable && !tailscaleEnabled);
  ui->comboInterface->setEnabled(writable && !tailscaleEnabled);
  ui->cbPreferPhysicalNetwork->setEnabled(writable && !tailscaleEnabled && ui->comboInterface->currentIndex() == 0);
  ui->comboLogLevel->setEnabled(writable);
  ui->groupLogToFile->setEnabled(writable);
  ui->rbAutoHide->setEnabled(writable);
  ui->rbShowOnStart->setEnabled(writable);
  ui->cbStartAtLogin->setEnabled(writable && StartupManager::isSupported());
  ui->cbAutoUpdate->setEnabled(writable);
  ui->cbPreventSleep->setEnabled(writable);
  ui->lineTlsCertPath->setEnabled(writable);
  ui->comboTlsKeyLength->setEnabled(writable);
  ui->rbCloseToTray->setEnabled(writable);
  ui->rbExitOnClose->setEnabled(writable);
  ui->cbRunEnterCommand->setEnabled(writable);
  ui->cbRunExitCommand->setEnabled(writable);
  ui->lineCommandEnter->setEnabled(writable && ui->cbRunEnterCommand->isChecked());
  ui->lineCommandExit->setEnabled(writable && ui->cbRunExitCommand->isChecked());
  ui->groupInputLanguage->setEnabled(writable);

  // Portable mode only ever applies to Windows.
  // Daemon options should only be available on Windows when *not* in portable mode.
  if (!Settings::isPortableMode()) {
    ui->groupService->setEnabled(writable);
    ui->cbElevateDaemon->setEnabled(writable && serviceChecked);
  } else if (ui->groupService->isVisibleTo(ui->tabAdvanced)) {
    ui->groupService->setVisible(false);
  }

  ui->widgetLogFilename->setEnabled(writable && logToFile);

  updateTlsControls();
}

void SettingsDialog::updateRequestedKeySize() const
{
  if (ui->comboTlsKeyLength->currentText() == Settings::value(Settings::Security::KeySize).toString())
    return;
  Settings::setValue(Settings::Security::KeySize, ui->comboTlsKeyLength->currentText());
}

void SettingsDialog::logLevelChanged()
{
  ui->lblDebugWarning->setVisible(ui->comboLogLevel->currentIndex() > static_cast<int>(LogLevel::Level::Info));
}

bool SettingsDialog::isModified() const
{
  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>();

  bool modified =
      (ui->sbPort->value() != Settings::value(Settings::Core::Port).toInt()) ||
      (ui->comboInterface->currentData().toString() != Settings::value(Settings::Core::Interface).toString()) ||
      (ui->cbPreferPhysicalNetwork->isChecked() != Settings::value(Settings::Core::PreferPhysicalNetwork).toBool()) ||
      (ui->groupTailscale->isChecked() != Settings::value(Settings::Core::UseTailscale).toBool()) ||
      (ui->comboLogLevel->currentData() != Settings::logLevelText()) ||
      (ui->groupLogToFile->isChecked() != Settings::value(Settings::Log::ToFile).toBool()) ||
      (ui->lineLogFilename->text() != Settings::value(Settings::Log::File).toString()) ||
      (ui->rbAutoHide->isChecked() != Settings::value(Settings::Gui::Autohide).toBool()) ||
      (ui->cbStartAtLogin->isChecked() != Settings::value(Settings::Gui::StartAtLogin).toBool()) ||
      (ui->cbPreventSleep->isChecked() != Settings::value(Settings::Core::PreventSleep).toBool()) ||
      (ui->rbCloseToTray->isChecked() != Settings::value(Settings::Gui::CloseToTray).toBool()) ||
      (ui->cbElevateDaemon->isChecked() != Settings::value(Settings::Daemon::Elevate).toBool()) ||
      (ui->cbAutoUpdate->isChecked() != Settings::value(Settings::Gui::AutoUpdateCheck).toBool()) ||
      (ui->cbGuiDebug->isChecked() != Settings::value(Settings::Log::GuiDebug).toBool()) ||
      (ui->cbShowVersion->isChecked() != Settings::value(Settings::Gui::ShowVersionInTitle).toBool()) ||
      (ui->rbIconMono->isChecked() != Settings::value(Settings::Gui::SymbolicTrayIcon).toBool()) ||
      (ui->groupService->isChecked() != (processMode == Settings::ProcessMode::Service)) ||
      (ui->lineTlsCertPath->text() != Settings::value(Settings::Security::Certificate).toString()) ||
      (ui->comboTlsKeyLength->currentText() != Settings::value(Settings::Security::KeySize).toString()) ||
      (ui->groupSecurity->isChecked() != Settings::value(Settings::Security::TlsEnabled).toBool()) ||
      (ui->cbRequireClientCert->isChecked() != Settings::value(Settings::Security::CheckPeers).toBool()) ||
      (ui->cbRunEnterCommand->isChecked() != Settings::value(Settings::Core::EnableEnterCommand).toBool()) ||
      (ui->cbRunExitCommand->isChecked() != Settings::value(Settings::Core::EnableExitCommand).toBool()) ||
      (ui->lineCommandEnter->text() != Settings::value(Settings::Core::ScreenEnterCommand).toString()) ||
      (ui->lineCommandExit->text() != Settings::value(Settings::Core::ScreenExitCommand).toString()) ||
      (ui->cbImeSync->isChecked() != Settings::value(Settings::Core::ImeSync).toBool()) ||
      (ui->comboCjkRawScancode->currentData().toString() != Settings::value(Settings::Client::CjkRawScancode).toString()
      ) ||
      (ui->comboEnterScreenLang->currentData().toString() != Settings::value(Settings::Core::EnterScreenLang).toString()
      ) ||
      (ui->cbClipboardNormalizeNfc->isChecked() != Settings::value(Settings::Client::ClipboardNormalizeNfc).toBool()) ||
      (ui->spinMacInterKeyDelayMicros->value() != Settings::value(Settings::Client::MacInterKeyDelayMicros).toInt()) ||
      (ui->cbFileTransferEnabled->isChecked() != Settings::value(Settings::FileTransfer::Enabled).toBool()) ||
      (ui->cbFileTransferReceive->isChecked() != Settings::value(Settings::FileTransfer::ReceiveEnabled).toBool()) ||
      (QDir::cleanPath(ui->lineFileTransferDirectory->text()) !=
       QDir::cleanPath(Settings::value(Settings::FileTransfer::DownloadDirectory).toString())) ||
      (I18N::nativeTo639Name(ui->comboLanguage->currentText()) != Settings::value(Settings::Core::Language).toString());

  return modified;
}

bool SettingsDialog::isDefault() const
{
  const auto processMode = Settings::defaultValue(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  const auto logLevelIndex =
      static_cast<int>(LogLevel::fromOption(Settings::defaultValue(Settings::Log::Level).toString()));

  return (
      (ui->sbPort->value() == Settings::defaultValue(Settings::Core::Port).toInt()) &&
      (ui->comboLogLevel->currentIndex() == logLevelIndex) &&
      (ui->groupLogToFile->isChecked() == Settings::defaultValue(Settings::Log::ToFile).toBool()) &&
      (ui->lineLogFilename->text() == Settings::defaultValue(Settings::Log::File).toString()) &&
      (ui->rbAutoHide->isChecked() == Settings::defaultValue(Settings::Gui::Autohide).toBool()) &&
      (ui->cbStartAtLogin->isChecked() == Settings::defaultValue(Settings::Gui::StartAtLogin).toBool()) &&
      (ui->cbPreventSleep->isChecked() == Settings::defaultValue(Settings::Core::PreventSleep).toBool()) &&
      (ui->rbCloseToTray->isChecked() == Settings::defaultValue(Settings::Gui::CloseToTray).toBool()) &&
      (ui->cbElevateDaemon->isChecked() == Settings::defaultValue(Settings::Daemon::Elevate).toBool()) &&
      (ui->cbAutoUpdate->isChecked() == Settings::defaultValue(Settings::Gui::AutoUpdateCheck).toBool()) &&
      (ui->cbGuiDebug->isChecked() == Settings::defaultValue(Settings::Log::GuiDebug).toBool()) &&
      (ui->cbShowVersion->isChecked() == Settings::defaultValue(Settings::Gui::ShowVersionInTitle).toBool()) &&
      (ui->rbIconMono->isChecked() == Settings::defaultValue(Settings::Gui::SymbolicTrayIcon).toBool()) &&
      (ui->groupService->isChecked() == (processMode == Settings::ProcessMode::Service)) &&
      (ui->comboInterface->currentIndex() == 0) &&
      (ui->cbPreferPhysicalNetwork->isChecked() ==
       Settings::defaultValue(Settings::Core::PreferPhysicalNetwork).toBool()) &&
      (ui->groupTailscale->isChecked() == Settings::defaultValue(Settings::Core::UseTailscale).toBool()) &&
      (ui->lineTlsCertPath->text() == Settings::defaultValue(Settings::Security::Certificate).toString()) &&
      (ui->comboTlsKeyLength->currentText() == Settings::defaultValue(Settings::Security::KeySize).toString()) &&
      (ui->groupSecurity->isChecked() == Settings::defaultValue(Settings::Security::TlsEnabled).toBool()) &&
      (ui->cbRequireClientCert->isChecked() == Settings::defaultValue(Settings::Security::CheckPeers).toBool()) &&
      (ui->lineCommandEnter->text() == Settings::defaultValue(Settings::Core::ScreenEnterCommand).toString()) &&
      (ui->lineCommandExit->text() == Settings::defaultValue(Settings::Core::ScreenExitCommand).toString()) &&
      (ui->cbRunEnterCommand->isChecked() == Settings::defaultValue(Settings::Core::EnableEnterCommand).toBool()) &&
      (ui->cbRunExitCommand->isChecked() == Settings::defaultValue(Settings::Core::EnableExitCommand).toBool()) &&
      (ui->cbImeSync->isChecked() == Settings::defaultValue(Settings::Core::ImeSync).toBool()) &&
      (ui->comboCjkRawScancode->currentData().toString() ==
       Settings::defaultValue(Settings::Client::CjkRawScancode).toString()) &&
      (ui->comboEnterScreenLang->currentData().toString() ==
       Settings::defaultValue(Settings::Core::EnterScreenLang).toString()) &&
      (ui->cbClipboardNormalizeNfc->isChecked() ==
       Settings::defaultValue(Settings::Client::ClipboardNormalizeNfc).toBool()) &&
      (ui->spinMacInterKeyDelayMicros->value() ==
       Settings::defaultValue(Settings::Client::MacInterKeyDelayMicros).toInt()) &&
      (ui->cbFileTransferEnabled->isChecked() == Settings::defaultValue(Settings::FileTransfer::Enabled).toBool()) &&
      (ui->cbFileTransferReceive->isChecked() == Settings::defaultValue(Settings::FileTransfer::ReceiveEnabled).toBool()
      ) &&
      (QDir::cleanPath(ui->lineFileTransferDirectory->text()) ==
       QDir::cleanPath(Settings::defaultValue(Settings::FileTransfer::DownloadDirectory).toString())) &&
      (ui->comboLanguage->currentText() == "English")
  );
}

void SettingsDialog::resetToDefault()
{
  m_acceptAfterTailscaleCheck = false;
  m_tailscaleIntegration->cancel();
  m_tailscaleQueryInProgress = false;
  m_loadingConfig = true;
  ui->sbPort->setValue(Settings::defaultValue(Settings::Core::Port).toInt());
  ui->comboLogLevel->setCurrentIndex(
      static_cast<int>(LogLevel::fromOption(Settings::defaultValue(Settings::Log::Level).toString()))
  );
  ui->groupLogToFile->setChecked(Settings::defaultValue(Settings::Log::ToFile).toBool());
  ui->lineLogFilename->setText(Settings::defaultValue(Settings::Log::File).toString());
  ui->cbPreventSleep->setChecked(Settings::defaultValue(Settings::Core::PreventSleep).toBool());
  ui->cbPreferPhysicalNetwork->setChecked(Settings::defaultValue(Settings::Core::PreferPhysicalNetwork).toBool());
  ui->groupTailscale->setChecked(Settings::defaultValue(Settings::Core::UseTailscale).toBool());
  ui->cbElevateDaemon->setChecked(Settings::defaultValue(Settings::Daemon::Elevate).toBool());
  ui->cbStartAtLogin->setChecked(Settings::defaultValue(Settings::Gui::StartAtLogin).toBool());
  ui->cbAutoUpdate->setChecked(Settings::defaultValue(Settings::Gui::AutoUpdateCheck).toBool());
  ui->cbGuiDebug->setChecked(Settings::defaultValue(Settings::Log::GuiDebug).toBool());
  ui->cbShowVersion->setChecked(Settings::defaultValue(Settings::Gui::ShowVersionInTitle).toBool());
  ui->cbRunEnterCommand->setChecked(Settings::defaultValue(Settings::Core::EnableEnterCommand).toBool());
  ui->cbRunExitCommand->setChecked(Settings::defaultValue(Settings::Core::EnableExitCommand).toBool());
  ui->lineCommandEnter->setText(Settings::defaultValue(Settings::Core::ScreenEnterCommand).toString());
  ui->lineCommandExit->setText(Settings::defaultValue(Settings::Core::ScreenExitCommand).toString());
  ui->cbImeSync->setChecked(Settings::defaultValue(Settings::Core::ImeSync).toBool());
  const auto defaultRawScancode = Settings::defaultValue(Settings::Client::CjkRawScancode).toString();
  const auto defaultEnterLang = Settings::defaultValue(Settings::Core::EnterScreenLang).toString();
  selectComboData(ui->comboCjkRawScancode, defaultRawScancode, defaultRawScancode);
  selectComboData(ui->comboEnterScreenLang, defaultEnterLang, defaultEnterLang);
  ui->cbClipboardNormalizeNfc->setChecked(Settings::defaultValue(Settings::Client::ClipboardNormalizeNfc).toBool());
  ui->spinMacInterKeyDelayMicros->setValue(Settings::defaultValue(Settings::Client::MacInterKeyDelayMicros).toInt());
  ui->cbFileTransferEnabled->setChecked(Settings::defaultValue(Settings::FileTransfer::Enabled).toBool());
  ui->cbFileTransferReceive->setChecked(Settings::defaultValue(Settings::FileTransfer::ReceiveEnabled).toBool());
  ui->lineFileTransferDirectory->setText(Settings::defaultValue(Settings::FileTransfer::DownloadDirectory).toString());

  const auto autoHide = Settings::defaultValue(Settings::Gui::Autohide).toBool();
  ui->rbAutoHide->setChecked(autoHide);
  ui->rbShowOnStart->setChecked(!autoHide);

  const auto closeToTray = Settings::defaultValue(Settings::Gui::CloseToTray).toBool();
  ui->rbCloseToTray->setChecked(closeToTray);
  ui->rbExitOnClose->setChecked(!closeToTray);

  const auto processMode = Settings::defaultValue(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  ui->groupService->setChecked(processMode == Settings::ProcessMode::Service);

  if (!deskflow::platform::isWindows())
    ui->groupService->setVisible(false);

  if (Settings::defaultValue(Settings::Gui::SymbolicTrayIcon).toBool())
    ui->rbIconMono->setChecked(true);
  else
    ui->rbIconColorful->setChecked(true);

  ui->lblDebugWarning->setVisible(false);

  ui->comboInterface->setCurrentIndex(0);

  m_previousInterface.clear();
  m_previousPreferPhysical = Settings::defaultValue(Settings::Core::PreferPhysicalNetwork).toBool();
  m_previousPort = Settings::defaultValue(Settings::Core::Port).toInt();
  m_previousNetworkCaptured = false;
  m_loadingConfig = false;
  m_tailscaleStatusChecked = false;
  ui->lblTailscaleStatus->setText(tailscaleStatusText());
  ui->lblTailscaleStatus->setToolTip(QString());
  qDebug() << "reset to default values";
  updateControls();
  setButtonBoxEnabledButtons();
}

void SettingsDialog::setButtonBoxEnabledButtons() const
{
  const bool modified = isModified();
  ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(modified && !m_tailscaleQueryInProgress);
  ui->buttonBox->button(QDialogButtonBox::Reset)->setEnabled(modified);
  ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setEnabled(!isDefault());
}

SettingsDialog::~SettingsDialog() = default;
