/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: MIT
 */

#include <QCoreApplication>
#include <QTimer>

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  if (qEnvironmentVariableIsSet("IEUM_TEST_CORE_EXIT_CODE")) {
    return qEnvironmentVariableIntValue("IEUM_TEST_CORE_EXIT_CODE");
  }
  QTimer::singleShot(30000, &app, &QCoreApplication::quit);
  return app.exec();
}
