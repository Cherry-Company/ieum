/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_9.h"

class ClientProxy1_10 : public ClientProxy1_9
{
public:
  ClientProxy1_10(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_10() override = default;

  int16_t protocolMinorVersion() const override
  {
    return 10;
  }
};
