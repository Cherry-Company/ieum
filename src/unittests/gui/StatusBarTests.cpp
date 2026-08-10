/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/widgets/StatusBar.h"

#include <QLabel>
#include <QTest>

class StatusBarTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void inputLanguageIndicatorHasQuietLifecycle()
  {
    StatusBar statusBar;
    auto *indicator = statusBar.findChild<QLabel *>(QStringLiteral("lblInputLanguage"));
    QVERIFY(indicator != nullptr);
    QVERIFY(!indicator->isVisible());

    const auto description = QStringLiteral("Input: A - This computer\nwindows.keylayout.0409");
    statusBar.setInputLanguageStatus(QStringLiteral("A"), description);
    QCOMPARE(indicator->text(), QStringLiteral("A"));
    QCOMPARE(indicator->toolTip(), description);
    QVERIFY(!indicator->isHidden());

    statusBar.clearInputLanguageStatus();
    QVERIFY(indicator->isHidden());
    QVERIFY(indicator->text().isEmpty());
    QVERIFY(indicator->toolTip().isEmpty());
  }
};

QTEST_MAIN(StatusBarTests)

#include "StatusBarTests.moc"
