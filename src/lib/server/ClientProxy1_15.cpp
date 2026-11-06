// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "ClientProxy1_15.h"
#include "deskflow/ProfessionalProtocol.h"
#include "server/Server.h"
#include <cstring>

bool ClientProxy1_15::sendProfessional(const deskflow::professional::Message &message)
{
  return deskflow::professional::writeFrame(getStream(), message);
}

bool ClientProxy1_15::parseMessage(const uint8_t *code)
{
  if (std::memcmp(code, "DPRF", 4) != 0)
    return ClientProxy1_14::parseMessage(code);
  const auto message = deskflow::professional::readFrame(getStream());
  if (!message)
    return false;
  // An unavailable feature/peer must not disconnect basic keyboard sharing.
  (void)getServer()->routeProfessional(this, *message);
  return true;
}
