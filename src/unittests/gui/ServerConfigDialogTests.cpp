/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ServerConfigDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

#include <utility>

namespace {

class TestServerConfigDialog final : public ServerConfigDialog
{
public:
  TestServerConfigDialog(ServerConfig &config, QString selectedConfigFile)
      : ServerConfigDialog(nullptr, config),
        m_selectedConfigFile(std::move(selectedConfigFile))
  {
  }

  bool browse()
  {
    return browseConfigFile();
  }

protected:
  QString chooseConfigFile([[maybe_unused]] const QString &filter) override
  {
    return m_selectedConfigFile;
  }

private:
  QString m_selectedConfigFile;
};

} // namespace

class ServerConfigDialogTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase()
  {
    QVERIFY(m_tempDir.isValid());
    m_settingsFile = m_tempDir.filePath(QStringLiteral("ieum-test.conf"));
    Settings::setSettingsFile(m_settingsFile);
  }

  void typedPathIsSynchronizedAndAccepted()
  {
    const auto originalPath = createConfigFile(QStringLiteral("typed-original.conf"));
    const auto typedPath = createConfigFile(QStringLiteral("typed.conf"));
    setExternalConfig(false, originalPath);

    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto *externalGroup = dialog.findChild<QGroupBox *>(QStringLiteral("groupExternalConfig"));
    auto *pathEdit = dialog.findChild<QLineEdit *>(QStringLiteral("lineConfigFile"));
    auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
    QVERIFY(externalGroup != nullptr);
    QVERIFY(pathEdit != nullptr);
    QVERIFY(buttons != nullptr);

    externalGroup->setChecked(true);
    pathEdit->setText(typedPath);

    QCOMPARE(config.configFile(), typedPath);
    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());

    dialog.accept();
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(config.configFile(), typedPath);
    QVERIFY(config.useExternalConfig());
  }

  void namedComputerIsPlacedOnChosenSideAndSaved()
  {
    setExternalConfig(false, createConfigFile(QStringLiteral("form.conf")));
    Settings::setValue(Settings::Core::ComputerName, "layout-host");
    ServerConfig config;
    config.addClient("existing-left");
    ServerConfigDialog dialog(nullptr, config);
    auto *name = dialog.findChild<QLineEdit *>("newComputerName");
    auto *side = dialog.findChild<QComboBox *>("newComputerSide");
    auto *grid = dialog.findChild<QTableView *>("screenSetupView");
    side->setCurrentIndex(side->findData(QPoint(1, 0)));
    name->setText("new-right");
    dialog.findChild<QPushButton *>("addComputer")->click();
    const auto target = grid->currentIndex();
    QCOMPARE(target.data().toString(), QStringLiteral("new-right"));
    QCOMPARE(target.siblingAtColumn(target.column() - 1).data().toString(), QStringLiteral("layout-host"));
    QVERIFY(name->text().isEmpty());
    QVERIFY(dialog.findChild<QPushButton *>("removeComputer")->isEnabled());
    dialog.accept();
    QVERIFY(config.screenExists("new-right"));
  }

  void invalidDuplicateAndOccupiedPositionsDoNotReplaceComputers()
  {
    setExternalConfig(false, createConfigFile(QStringLiteral("form-errors.conf")));
    Settings::setValue(Settings::Core::ComputerName, "layout-host");
    ServerConfig config;
    config.addClient("existing-left");
    ServerConfigDialog dialog(nullptr, config);
    auto *name = dialog.findChild<QLineEdit *>("newComputerName");
    auto *side = dialog.findChild<QComboBox *>("newComputerSide");
    auto *feedback = dialog.findChild<QLabel *>("layoutFeedback");
    auto *add = dialog.findChild<QPushButton *>("addComputer");
    name->setText("bad name");
    add->click();
    QVERIFY(feedback->text().contains("not allowed"));
    name->setText("existing-left");
    add->click();
    QVERIFY(feedback->text().contains("already"));
    side->setCurrentIndex(side->findData(QPoint(-1, 0)));
    name->setText("replacement");
    add->click();
    QVERIFY(feedback->text().contains("occupied"));
    dialog.accept();
    QVERIFY(config.screenExists("existing-left"));
    QVERIFY(!config.screenExists("replacement"));
    QVERIFY(!config.screenExists("bad name"));
  }

  void hostCannotBeRemovedAndArrowMovementWorks()
  {
    setExternalConfig(false, createConfigFile(QStringLiteral("form-move.conf")));
    Settings::setValue(Settings::Core::ComputerName, "layout-host");
    ServerConfig config;
    config.addClient("existing-left");
    ServerConfigDialog dialog(nullptr, config);
    auto *grid = dialog.findChild<QTableView *>("screenSetupView");
    QModelIndex host, client;
    for (int row = 0; row < grid->model()->rowCount(); ++row) {
      for (int column = 0; column < grid->model()->columnCount(); ++column) {
        const auto index = grid->model()->index(row, column);
        if (index.data().toString() == QStringLiteral("layout-host"))
          host = index;
        if (index.data().toString() == QStringLiteral("existing-left"))
          client = index;
      }
    }
    QVERIFY(host.isValid());
    QVERIFY(client.isValid());
    grid->setCurrentIndex(host);
    QVERIFY(!dialog.findChild<QPushButton *>("removeComputer")->isEnabled());
    grid->setCurrentIndex(client);
    for (auto *button : dialog.findChildren<QToolButton *>()) {
      if (button->accessibleName() == QStringLiteral("Move up"))
        button->click();
    }
    QCOMPARE(grid->currentIndex().row(), client.row() - 1);
    QCOMPARE(grid->currentIndex().data().toString(), QStringLiteral("existing-left"));
    dialog.findChild<QPushButton *>("removeComputer")->click();
    dialog.accept();
    QVERIFY(config.screenExists("layout-host"));
    QVERIFY(!config.screenExists("existing-left"));
  }

  void cancelRestoresTypedPath()
  {
    const auto originalPath = createConfigFile(QStringLiteral("cancel-original.conf"));
    const auto typedPath = createConfigFile(QStringLiteral("cancel-typed.conf"));
    setExternalConfig(true, originalPath);
    Settings::setValue(Settings::Core::ComputerName, QStringLiteral("cancel-server"));

    ServerConfig config;
    config.addClient(QStringLiteral("cancel-client"));
    ServerConfigDialog dialog(nullptr, config);
    auto *pathEdit = dialog.findChild<QLineEdit *>(QStringLiteral("lineConfigFile"));
    auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
    QVERIFY(pathEdit != nullptr);
    QVERIFY(buttons != nullptr);
    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());

    pathEdit->setText(typedPath);
    QCOMPARE(config.configFile(), typedPath);
    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());

    dialog.reject();
    QCOMPARE(config.configFile(), originalPath);
    QVERIFY(config.useExternalConfig());
  }

  void invalidPathIsRejected_data()
  {
    QTest::addColumn<QString>("invalidPath");

    QTest::newRow("missing-file") << m_tempDir.filePath(QStringLiteral("missing.conf"));
    QTest::newRow("existing-directory") << m_tempDir.path();
  }

  void invalidPathIsRejected()
  {
    QFETCH(QString, invalidPath);

    const auto originalPath = createConfigFile(QStringLiteral("invalid-original.conf"));
    setExternalConfig(false, originalPath);

    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto *externalGroup = dialog.findChild<QGroupBox *>(QStringLiteral("groupExternalConfig"));
    auto *pathEdit = dialog.findChild<QLineEdit *>(QStringLiteral("lineConfigFile"));
    auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
    QVERIFY(externalGroup != nullptr);
    QVERIFY(pathEdit != nullptr);
    QVERIFY(buttons != nullptr);

    externalGroup->setChecked(true);
    pathEdit->setText(invalidPath);

    QCOMPARE(config.configFile(), invalidPath);
    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
    QCOMPARE(pathEdit->property("configPathValid").toBool(), false);
    QVERIFY(!pathEdit->toolTip().isEmpty());
    QCOMPARE(pathEdit->accessibleDescription(), pathEdit->toolTip());

    dialog.accept();
    QVERIFY(dialog.result() != QDialog::Accepted);
  }

  void browseSynchronizesTheSelectedFile()
  {
    const auto originalPath = createConfigFile(QStringLiteral("browse-original.conf"));
    const auto selectedPath = createConfigFile(QStringLiteral("browse-selected.conf"));
    setExternalConfig(false, originalPath);

    ServerConfig config;
    TestServerConfigDialog dialog(config, selectedPath);
    auto *externalGroup = dialog.findChild<QGroupBox *>(QStringLiteral("groupExternalConfig"));
    auto *pathEdit = dialog.findChild<QLineEdit *>(QStringLiteral("lineConfigFile"));
    auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
    QVERIFY(externalGroup != nullptr);
    QVERIFY(pathEdit != nullptr);
    QVERIFY(buttons != nullptr);

    externalGroup->setChecked(true);
    QVERIFY(dialog.browse());

    QCOMPARE(pathEdit->text(), selectedPath);
    QCOMPARE(config.configFile(), selectedPath);
    QCOMPARE(pathEdit->property("configPathValid").toBool(), true);
    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
  }

private:
  QString createConfigFile(const QString &name)
  {
    const auto path = m_tempDir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return {};
    }
    file.write("section: screens\n");
    file.close();
    return path;
  }

  static void setExternalConfig(const bool enabled, const QString &path)
  {
    Settings::setValue(Settings::Server::ExternalConfig, enabled);
    Settings::setValue(Settings::Server::ExternalConfigFile, path);
  }

  QTemporaryDir m_tempDir;
  QString m_settingsFile;
};

QTEST_MAIN(ServerConfigDialogTests)

#include "ServerConfigDialogTests.moc"
