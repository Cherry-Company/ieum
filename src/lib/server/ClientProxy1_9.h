/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_8.h"

class ClientProxy1_9 : public ClientProxy1_8
{
public:
  ClientProxy1_9(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_9() override = default;

  int16_t protocolMinorVersion() const override
  {
    return 9;
  }

  void inputLanguageControl(deskflow::InputLanguageAction action, const std::string &target) override;
  deskflow::InputLanguageStatus inputLanguageStatus() const override;

protected:
  bool parseMessage(const uint8_t *code) override;

private:
  deskflow::InputLanguageStatus m_inputLanguageStatus;
};
