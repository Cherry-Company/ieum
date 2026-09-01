/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_13.h"

class ClientProxy1_14 : public ClientProxy1_13
{
public:
  ClientProxy1_14(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_14() override = default;

  int16_t protocolMinorVersion() const override
  {
    return 14;
  }

  bool sendFileTransferEdge(const deskflow::filetransfer::FileTransferEdgeMessage &message) override;

protected:
  bool parseMessage(const uint8_t *code) override;

private:
  bool recvFileTransferEdge();
};
