/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferControlCodec.h"

#include <string>
#include <string_view>

namespace deskflow::filetransfer {

enum class FileTransferRouteError
{
  None,
  InvalidMessage,
  SenderNotParticipant,
  InvalidSenderRole,
  DestinationUnavailable,
  DestinationUnsupported,
  DeliveryFailed,
};

struct FileTransferRouteResult
{
  FileTransferRouteError error = FileTransferRouteError::None;
  std::string destinationScreen;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferRouteError::None;
  }
};

[[nodiscard]] FileTransferRouteResult resolveFileTransferControlDestination(
    const FileTransferControlMessage &message, std::string_view senderScreen,
    const FileTransferControlLimits &limits = {}
);

} // namespace deskflow::filetransfer
