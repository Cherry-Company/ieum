/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_12.h"

class ClientProxy1_13 : public ClientProxy1_12
{
public:
  ClientProxy1_13(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_13() override = default;

  int16_t protocolMinorVersion() const override
  {
    return 13;
  }

  bool sendFileTransferData(const deskflow::filetransfer::FileTransferDataMessage &message) override;

protected:
  bool parseMessage(const uint8_t *code) override;

private:
  bool recvFileTransferData();
};
