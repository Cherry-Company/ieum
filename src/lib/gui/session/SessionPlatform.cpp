// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "SessionPlatform.h"
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <cmath>

namespace deskflow::gui::session {
QJsonObject Input::toJson() const
{
  return {{"kind", static_cast<int>(kind)}, {"code", code}, {"down", down}, {"x", x}, {"y", y}};
}
std::optional<Input> Input::fromJson(const QJsonObject &object)
{
  if (object.size() != 5 || !object.value("down").isBool())
    return std::nullopt;
  for (const auto *field : {"kind", "code", "x", "y"}) {
    const auto value = object.value(QLatin1String(field));
    if (!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble() != value.toInt())
      return std::nullopt;
  }
  Input input{
      static_cast<Kind>(object.value("kind").toInt()), object.value("code").toInt(), object.value("down").toBool(),
      object.value("x").toInt(), object.value("y").toInt()
  };
  switch (input.kind) {
  case Kind::Key:
    if (input.code <= 0 || input.code > 0x1ff || (input.code & 0xff) == 0 || input.x || input.y)
      return std::nullopt;
    break;
  case Kind::Button:
    if (input.code < 1 || input.code > 5 || input.x || input.y)
      return std::nullopt;
    break;
  case Kind::Move:
    if (input.code || input.down || input.x < 0 || input.x > 65535 || input.y < 0 || input.y > 65535)
      return std::nullopt;
    break;
  case Kind::Wheel:
    if (input.code || input.down || input.x < -1200 || input.x > 1200 || input.y < -1200 || input.y > 1200)
      return std::nullopt;
    break;
  default:
    return std::nullopt;
  }
  return input;
}

QImage SessionPlatform::screen()
{
  auto *screen = QGuiApplication::primaryScreen();
  if (!screen || locked())
    return {};
  return screen->grabWindow(0).toImage();
}
} // namespace deskflow::gui::session
