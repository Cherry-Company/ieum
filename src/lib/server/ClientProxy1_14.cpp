/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_14.h"

#include "base/Log.h"
#include "deskflow/FileTransferEdgeProtocol.h"
#include "deskflow/ProtocolTypes.h"
#include "server/Server.h"

#include <cstring>

ClientProxy1_14::ClientProxy1_14(
    const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events
)
    : ClientProxy1_13(name, stream, server, events)
{
}

bool ClientProxy1_14::sendFileTransferEdge(const deskflow::filetransfer::FileTransferEdgeMessage &message)
{
  const auto result = deskflow::filetransfer::writeFileTransferEdgeFrame(getStream(), message);
  if (!result.ok()) {
    LOG_WARN("refusing to send invalid file-transfer edge message to client \"%s\"", getName().c_str());
    return false;
  }
  return true;
}

bool ClientProxy1_14::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDFileTransferEdge, 4) == 0) {
    return recvFileTransferEdge();
  }
  return ClientProxy1_13::parseMessage(code);
}

bool ClientProxy1_14::recvFileTransferEdge()
{
  auto received = deskflow::filetransfer::readFileTransferEdgeFrame(getStream());
  if (!received.ok()) {
    LOG_WARN(
        "client \"%s\" sent an invalid file-transfer edge frame: %d", getName().c_str(),
        static_cast<int>(received.error)
    );
    return false;
  }

  if (!getServer()->handleFileTransferEdge(this, *received.message)) {
    LOG_WARN("could not route file-transfer edge message from client \"%s\"", getName().c_str());
    return false;
  }
  return true;
}
