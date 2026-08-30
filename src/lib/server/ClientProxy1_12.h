/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_11.h"

class ClientProxy1_12 : public ClientProxy1_11
{
public:
  ClientProxy1_12(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_12() override = default;

  int16_t protocolMinorVersion() const override
  {
    return 12;
  }

  bool sendFileTransferControl(const deskflow::filetransfer::FileTransferControlMessage &message) override;

protected:
  bool parseMessage(const uint8_t *code) override;

private:
  bool recvFileTransferControl();
};
