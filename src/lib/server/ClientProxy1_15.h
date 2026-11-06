// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#pragma once
#include "server/ClientProxy1_14.h"

class ClientProxy1_15 : public ClientProxy1_14
{
public:
  using ClientProxy1_14::ClientProxy1_14;
  int16_t protocolMinorVersion() const override
  {
    return 15;
  }
  bool sendProfessional(const deskflow::professional::Message &message) override;

protected:
  bool parseMessage(const uint8_t *code) override;
};
