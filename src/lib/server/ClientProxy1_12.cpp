/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_12.h"

#include "base/Log.h"
#include "deskflow/FileTransferControlProtocol.h"
#include "deskflow/FileTransferControlRouting.h"
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

ClientProxy1_12::ClientProxy1_12(
    const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events
)
    : ClientProxy1_11(name, stream, server, events)
{
}

bool ClientProxy1_12::sendFileTransferControl(const deskflow::filetransfer::FileTransferControlMessage &message)
{
  const auto result = deskflow::filetransfer::writeFileTransferControlFrame(getStream(), message);
  if (!result.ok()) {
    LOG_WARN("refusing to send invalid file-transfer control message to client \"%s\"", getName().c_str());
    return false;
  }
  return true;
}

bool ClientProxy1_12::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDFileTransferControl, 4) == 0) {
    return recvFileTransferControl();
  }
  return ClientProxy1_11::parseMessage(code);
}

bool ClientProxy1_12::recvFileTransferControl()
{
  auto received = deskflow::filetransfer::readFileTransferControlFrame(getStream());
  if (!received.ok()) {
    LOG_WARN(
        "client \"%s\" sent an invalid file-transfer control frame: %d", getName().c_str(),
        static_cast<int>(received.error)
    );
    return false;
  }

  const auto routed = getServer()->routeFileTransferControl(this, *received.message);
  if (!routed.ok()) {
    LOG_WARN(
        "could not route file-transfer control message from client \"%s\": %d", getName().c_str(),
        static_cast<int>(routed.error)
    );
    return !isPeerViolation(routed.error);
  }
  return true;
}
