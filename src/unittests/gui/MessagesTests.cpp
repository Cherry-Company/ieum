/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/Logger.h"
#include "gui/Messages.h"

#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTest>
#include <thread>

class MessagesTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void workerErrorsAreDeferredAndDeduplicated()
  {
    QStandardPaths::setTestModeEnabled(true);
    deskflow::gui::Logger::instance();
    const auto dialogs = [] {
      QList<QMessageBox *> result;
      for (auto *widget : QApplication::topLevelWidgets()) {
        if (auto *dialog = qobject_cast<QMessageBox *>(widget);
            dialog && dialog->property("diagnosticMessage").isValid()) {
          result.append(dialog);
        }
      }
      return result;
    };
    std::thread worker([] {
      for (int i = 0; i < 32; ++i) {
        deskflow::gui::messages::messageHandler(QtCriticalMsg, {}, QStringLiteral("worker diagnostic regression"));
      }
    });
    worker.join();
    QVERIFY(dialogs().isEmpty());
    QTRY_COMPARE(dialogs().size(), 1);
    auto *dialog = dialogs().first();
    QCOMPARE(dialog->thread(), qApp->thread());
    dialog->done(QMessageBox::Ok);
    QTRY_VERIFY(dialogs().isEmpty());
  }

  void shutdownDoesNotOpenErrorDialogs()
  {
    qApp->setProperty("ieumShuttingDown", true);
    deskflow::gui::messages::messageHandler(QtCriticalMsg, {}, QStringLiteral("shutdown diagnostic regression"));
    QCoreApplication::processEvents();
    for (auto *widget : QApplication::topLevelWidgets()) {
      QVERIFY(!widget->property("diagnosticMessage").isValid());
    }
    qApp->setProperty("ieumShuttingDown", false);
  }
};

QTEST_MAIN(MessagesTests)
#include "MessagesTests.moc"
