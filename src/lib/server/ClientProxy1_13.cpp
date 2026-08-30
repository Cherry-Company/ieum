/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_13.h"

#include "base/Log.h"
#include "deskflow/FileTransferDataProtocol.h"
#include "deskflow/ProtocolTypes.h"
#include "server/Server.h"

#include <cstring>

namespace {

bool isPeerViolation(deskflow::filetransfer::FileTransferRouteError error)
{
  using enum deskflow::filetransfer::FileTransferRouteError;
  return error == InvalidMessage || error == SenderNotParticipant || error == InvalidSenderRole;
}

} // namespace

ClientProxy1_13::ClientProxy1_13(
    const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events
)
    : ClientProxy1_12(name, stream, server, events)
{
}

bool ClientProxy1_13::sendFileTransferData(const deskflow::filetransfer::FileTransferDataMessage &message)
{
  const auto result = deskflow::filetransfer::writeFileTransferDataFrame(getStream(), message);
  if (!result.ok()) {
    LOG_WARN("refusing to send invalid file-transfer data message to client \"%s\"", getName().c_str());
    return false;
  }
  return true;
}

bool ClientProxy1_13::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDFileTransferData, 4) == 0) {
    return recvFileTransferData();
  }
  return ClientProxy1_12::parseMessage(code);
}

bool ClientProxy1_13::recvFileTransferData()
{
  auto received = deskflow::filetransfer::readFileTransferDataFrame(getStream());
  if (!received.ok()) {
    LOG_WARN(
        "client \"%s\" sent an invalid file-transfer data frame: %d", getName().c_str(),
        static_cast<int>(received.error)
    );
    return false;
  }

  const auto routed = getServer()->routeFileTransferData(this, *received.message);
  if (!routed.ok()) {
    LOG_WARN(
        "could not route file-transfer data message from client \"%s\": %d", getName().c_str(),
        static_cast<int>(routed.error)
    );
    return !isPeerViolation(routed.error);
  }
  return true;
}
