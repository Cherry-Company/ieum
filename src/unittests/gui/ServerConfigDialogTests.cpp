/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ServerConfigDialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

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
