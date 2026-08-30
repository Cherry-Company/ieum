/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferControlCodec.h"

#include <optional>

namespace deskflow {
class IStream;
}

namespace deskflow::filetransfer {

enum class FileTransferFrameError
{
  None,
  TruncatedLength,
  FrameTooLarge,
  TruncatedPayload,
  InvalidPayload,
};

struct FileTransferFrameReadResult
{
  FileTransferFrameError error = FileTransferFrameError::None;
  FileTransferCodecError codecError = FileTransferCodecError::None;
  std::optional<FileTransferControlMessage> message;
  OfferValidation validation;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferFrameError::None;
  }
};

// Writes the complete DFTC message code, outer length, and encoded payload.
[[nodiscard]] FileTransferControlEncodeResult writeFileTransferControlFrame(
    IStream *stream, const FileTransferControlMessage &message, const FileTransferControlLimits &limits = {}
);

// Reads the outer length and payload after a dispatcher has consumed DFTC.
[[nodiscard]] FileTransferFrameReadResult
readFileTransferControlFrame(IStream *stream, const FileTransferControlLimits &limits = {});

} // namespace deskflow::filetransfer
