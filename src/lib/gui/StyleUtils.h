/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDir>
#include <QFileInfoList>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QPalette>
#include <QStyleHints>
#include <QWidget>

#include "common/Constants.h"

namespace deskflow::gui {

/**
 * @brief Detects dark mode in a universal manner (all Qt versions).
 * Until better platform support is added, it's more reliable to use the old way (compare text and window lightness),
 * because the newer versions in Qt 6.5+ are not always correct and some return `UnknownScheme`.
 * https://www.qt.io/blog/dark-mode-on-windows-11-with-qt-6.5
 */
inline bool isDarkMode()
{
  const QPalette defaultPalette;
  const auto text = defaultPalette.color(QPalette::WindowText);
  const auto window = defaultPalette.color(QPalette::Window);
  return text.lightness() > window.lightness();
}
/**
 * @brief get a string for the iconMode
 * @returns "dark" or "light"
 */
inline QString iconMode()
{
  return isDarkMode() ? QStringLiteral("dark") : QStringLiteral("light");
}

inline void updateIconTheme()
{
  // Sets the fallback icon path and fallback theme
  const auto themeName = QStringLiteral("%1-%2").arg(kAppId, iconMode());
  if (QIcon::themeName().isEmpty() || QIcon::themeName().startsWith(kAppId))
    QIcon::setThemeName(themeName);
  else
    QIcon::setFallbackThemeName(themeName);
  QIcon::setFallbackSearchPaths({QStringLiteral(":/icons/%1").arg(themeName)});
}

inline QString cssColor(const QColor &color, int alpha = 255)
{
  return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
}

inline QColor blendedColor(const QColor &first, const QColor &second, qreal secondWeight)
{
  const auto firstWeight = 1.0 - secondWeight;
  return {
      qRound((first.red() * firstWeight) + (second.red() * secondWeight)),
      qRound((first.green() * firstWeight) + (second.green() * secondWeight)),
      qRound((first.blue() * firstWeight) + (second.blue() * secondWeight))
  };
}

inline void applyIeumMainWindowStyle(QWidget &window)
{
  const auto palette = QGuiApplication::palette();
  const auto windowColor = palette.color(QPalette::Window);
  const auto baseColor = palette.color(QPalette::Base);
  const auto textColor = palette.color(QPalette::WindowText);
  const auto secondaryText = palette.color(QPalette::PlaceholderText);
  const auto accent = palette.color(QPalette::Highlight);
  const auto accentText = palette.color(QPalette::HighlightedText);
  const auto border = blendedColor(windowColor, textColor, isDarkMode() ? 0.24 : 0.16);
  const auto hover = blendedColor(windowColor, textColor, isDarkMode() ? 0.12 : 0.07);
  const auto disabled = blendedColor(windowColor, textColor, 0.34);

  const auto styleSheet = QStringLiteral(R"(
QMainWindow#MainWindow, QWidget#topLevelWidget {
  background-color: %1;
  color: %3;
}
QWidget#identityBar {
  background-color: %2;
  border: 1px solid %5;
  border-radius: 8px;
}
QLabel#lblProductName {
  font-size: 22px;
  font-weight: 600;
}
QLabel#lblTagline, QLabel#lblIpAddresses, QLabel[secondary="true"] {
  color: %4;
}
QLabel[sectionHeading="true"] {
  font-size: 13px;
  font-weight: 600;
}
QWidget#deviceBand {
  border-bottom: 1px solid %5;
}
QWidget#modeSurface {
  background-color: %2;
  border: 1px solid %5;
  border-radius: 8px;
}
QWidget#modeSwitcher {
  background-color: %6;
  border: 1px solid %5;
  border-radius: 8px;
}
QRadioButton#rbModeServer, QRadioButton#rbModeClient {
  background-color: transparent;
  border: 1px solid transparent;
  border-radius: 7px;
  padding: 10px 16px;
  min-height: 34px;
}
QRadioButton#rbModeServer::indicator, QRadioButton#rbModeClient::indicator {
  width: 0px;
  height: 0px;
}
QRadioButton#rbModeServer:hover:!checked, QRadioButton#rbModeClient:hover:!checked {
  background-color: %7;
}
QRadioButton#rbModeServer:checked, QRadioButton#rbModeClient:checked {
  background-color: %8;
  border-color: %8;
  color: %9;
}
QLineEdit#lineHostname, QLineEdit#lineEditName {
  background-color: %10;
  border: 1px solid %5;
  border-radius: 7px;
  padding: 6px 9px;
  selection-background-color: %8;
  selection-color: %9;
}
QLineEdit#lineHostname:focus, QLineEdit#lineEditName:focus {
  border-color: %8;
}
QPushButton#btnToggleCore {
  background-color: %8;
  border: 1px solid %8;
  border-radius: 8px;
  color: %9;
  font-weight: 600;
  min-width: 112px;
  padding: 7px 16px;
}
QPushButton#btnToggleCore:hover {
  background-color: %11;
  border-color: %11;
}
QPushButton#btnToggleCore:disabled {
  background-color: %12;
  border-color: %12;
}
QPushButton#btnConfigureServer, QPushButton#btnConfigureClient {
  border-radius: 7px;
  padding: 6px 12px;
}
QPushButton#btnEditName, QPushButton#btnSaveServerConfig, QPushButton#btnRestartCore {
  background-color: transparent;
  border: 1px solid transparent;
  border-radius: 7px;
}
QPushButton#btnEditName:hover, QPushButton#btnSaveServerConfig:hover, QPushButton#btnRestartCore:hover {
  background-color: %7;
  border-color: %5;
}
QStatusBar {
  background-color: %2;
  border-top: 1px solid %5;
}
)")
                              .arg(cssColor(windowColor))
                              .arg(cssColor(baseColor, isDarkMode() ? 174 : 205))
                              .arg(cssColor(textColor))
                              .arg(cssColor(secondaryText))
                              .arg(cssColor(border))
                              .arg(cssColor(windowColor, isDarkMode() ? 150 : 178))
                              .arg(cssColor(hover))
                              .arg(cssColor(accent))
                              .arg(cssColor(accentText))
                              .arg(cssColor(baseColor))
                              .arg(cssColor(blendedColor(accent, textColor, isDarkMode() ? 0.12 : 0.08)))
                              .arg(cssColor(disabled));

  if (window.styleSheet() != styleSheet)
    window.setStyleSheet(styleSheet);
}

inline void applyIeumDialogStyle(QWidget &dialog)
{
  const auto palette = dialog.palette();
  const auto windowColor = palette.color(QPalette::Window);
  const auto baseColor = palette.color(QPalette::Base);
  const auto textColor = palette.color(QPalette::WindowText);
  const auto border = blendedColor(windowColor, textColor, isDarkMode() ? 0.24 : 0.16);

  dialog.setStyleSheet(QStringLiteral(R"(
QDialog {
  background-color: %1;
}
QWidget#frameLogo, QTabWidget::pane, QGroupBox {
  background-color: %2;
  border: 1px solid %3;
  border-radius: 8px;
}
QGroupBox {
  margin-top: 12px;
  padding: 10px 8px 8px 8px;
  font-weight: 600;
}
QGroupBox::title {
  subcontrol-origin: margin;
  left: 10px;
  padding: 0 4px;
}
QScrollArea {
  background-color: transparent;
  border: 0px;
}
)")
                           .arg(cssColor(windowColor), cssColor(baseColor, isDarkMode() ? 174 : 205), cssColor(border))
  );
}
} // namespace deskflow::gui

inline QFont fixedFont()
{
#if defined(Q_OS_WIN)
  QFont f({"Hack", "Liberation Mono", "Monospace", "Andale Mono"});
  f.setStyleHint(QFont::Monospace);
#else
  QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#endif

#if defined(Q_OS_MAC)
  f.setPointSize(12);
#endif
  return f;
}
