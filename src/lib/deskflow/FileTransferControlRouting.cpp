/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferControlRouting.h"

#include <type_traits>
#include <variant>

namespace deskflow::filetransfer {
namespace {

FileTransferRouteResult routeFailure(FileTransferRouteError error)
{
  return {.error = error, .destinationScreen = {}};
}

} // namespace

FileTransferRouteResult resolveFileTransferControlDestination(
    const FileTransferControlMessage &message, std::string_view senderScreen, const FileTransferControlLimits &limits
)
{
  if (!encodeFileTransferControl(message, limits).ok()) {
    return routeFailure(FileTransferRouteError::InvalidMessage);
  }

  return std::visit(
      [&](const auto &value) -> FileTransferRouteResult {
        const bool isSource = senderScreen == value.sourceScreen;
        const bool isTarget = senderScreen == value.targetScreen;
        if (!isSource && !isTarget) {
          return routeFailure(FileTransferRouteError::SenderNotParticipant);
        }

        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferOffer>) {
          if (!isSource) {
            return routeFailure(FileTransferRouteError::InvalidSenderRole);
          }
        } else if constexpr (std::is_same_v<Value, FileTransferDecision>) {
          if (!isTarget) {
            return routeFailure(FileTransferRouteError::InvalidSenderRole);
          }
        }

        return {
            .error = FileTransferRouteError::None,
            .destinationScreen = isSource ? value.targetScreen : value.sourceScreen,
        };
      },
      message
  );
}

} // namespace deskflow::filetransfer
