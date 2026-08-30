/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferDataRouting.h"

#include <variant>

namespace deskflow::filetransfer {

FileTransferRouteResult resolveFileTransferDataDestination(
    const FileTransferDataMessage &message, std::string_view senderScreen, const FileTransferDataLimits &limits
)
{
  if (!encodeFileTransferData(message, limits).ok()) {
    return {.error = FileTransferRouteError::InvalidMessage, .destinationScreen = {}};
  }

  return std::visit(
      [&](const auto &value) -> FileTransferRouteResult {
        const auto &route = value.route;
        if (senderScreen != route.sourceScreen && senderScreen != route.targetScreen) {
          return {.error = FileTransferRouteError::SenderNotParticipant, .destinationScreen = {}};
        }
        if (senderScreen != route.sourceScreen) {
          return {.error = FileTransferRouteError::InvalidSenderRole, .destinationScreen = {}};
        }
        return {.error = FileTransferRouteError::None, .destinationScreen = route.targetScreen};
      },
      message
  );
}

} // namespace deskflow::filetransfer
