/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/IPlatformScreen.h"

class FakePlatformScreen : public IPlatformScreen
{
public:
  explicit FakePlatformScreen(IEventQueue *events, bool primary = false) : IPlatformScreen(events), m_primary(primary)
  {
  }

  deskflow::DisplayGeometry shape{0, 0, 1920, 1080};
  deskflow::DisplayLayout displays{shape};
  int32_t cursorX = 500;
  int32_t cursorY = 500;
  bool fullscreen = false;

  void *getEventTarget() const override
  {
    return const_cast<FakePlatformScreen *>(this);
  }
  bool getClipboard(ClipboardID, IClipboard *) const override
  {
    return false;
  }
  void getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const override
  {
    x = shape.x;
    y = shape.y;
    width = shape.width;
    height = shape.height;
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = cursorX;
    y = cursorY;
  }
  deskflow::DisplayLayout getDisplayLayout() const override
  {
    return displays;
  }
  bool isForegroundFullscreen() const override
  {
    return fullscreen;
  }
  bool isPrimary() const override
  {
    return m_primary;
  }
  void reconfigure(uint32_t sides) override
  {
    m_sides = sides;
  }
  uint32_t activeSides() override
  {
    return m_sides;
  }
  void warpCursor(int32_t x, int32_t y) override
  {
    cursorX = x;
    cursorY = y;
  }
  uint32_t registerHotKey(KeyID, KeyModifierMask) override
  {
    return 1;
  }
  void unregisterHotKey(uint32_t) override
  {
  }
  void fakeInputBegin() override
  {
  }
  void fakeInputEnd() override
  {
  }
  int32_t getJumpZoneSize() const override
  {
    return 1;
  }
  bool isAnyMouseButtonDown(uint32_t &) const override
  {
    return false;
  }
  void getCursorCenter(int32_t &x, int32_t &y) const override
  {
    x = shape.x + shape.width / 2;
    y = shape.y + shape.height / 2;
  }
  void fakeMouseButton(ButtonID, bool) override
  {
  }
  void fakeMouseMove(int32_t x, int32_t y) override
  {
    warpCursor(x, y);
  }
  void fakeMouseRelativeMove(int32_t, int32_t) const override
  {
  }
  void fakeMouseWheel(ScrollDelta) const override
  {
  }
  void updateKeyMap() override
  {
  }
  void updateKeyState() override
  {
  }
  void setHalfDuplexMask(KeyModifierMask) override
  {
  }
  void fakeKeyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override
  {
  }
  bool fakeKeyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override
  {
    return false;
  }
  bool fakeKeyUp(KeyButton) override
  {
    return false;
  }
  void fakeAllKeysUp() override
  {
  }
  bool fakeCtrlAltDel() override
  {
    return false;
  }
  bool isKeyDown(KeyButton) const override
  {
    return false;
  }
  KeyModifierMask getActiveModifiers() const override
  {
    return 0;
  }
  KeyModifierMask pollActiveModifiers() const override
  {
    return 0;
  }
  int32_t pollActiveGroup() const override
  {
    return 0;
  }
  void pollPressedKeys(KeyButtonSet &) const override
  {
  }
  void enable() override
  {
  }
  void disable() override
  {
  }
  void enter() override
  {
  }
  bool canLeave() override
  {
    return true;
  }
  void leave() override
  {
  }
  bool setClipboard(ClipboardID, const IClipboard *) override
  {
    return true;
  }
  void checkClipboards() override
  {
  }
  void openScreensaver(bool) override
  {
  }
  void closeScreensaver() override
  {
  }
  void screensaver(bool) override
  {
  }
  void resetOptions() override
  {
  }
  void setOptions(const OptionsList &) override
  {
  }
  void setSequenceNumber(uint32_t) override
  {
  }
  std::string getSecureInputApp() const override
  {
    return {};
  }

protected:
  void handleSystemEvent(const Event &) override
  {
  }

private:
  bool m_primary;
  uint32_t m_sides = 0;
};
