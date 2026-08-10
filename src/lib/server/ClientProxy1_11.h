/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_10.h"

class ClientProxy1_11 : public ClientProxy1_10
{
public:
  ClientProxy1_11(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_11() override = default;

  int16_t protocolMinorVersion() const override
  {
    return 11;
  }

  deskflow::DisplayLayout getDisplayLayout() const override;
  bool isForegroundFullscreen() const override;

protected:
  bool parseMessage(const uint8_t *code) override;

private:
  bool recvDisplayLayout();
  bool recvForegroundFullscreen();

  deskflow::DisplayLayout m_displayLayout;
  bool m_foregroundFullscreen = false;
};
