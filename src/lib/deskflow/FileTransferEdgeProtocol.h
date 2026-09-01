/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferEdgeCodec.h"

#include <optional>

namespace deskflow {
class IStream;
}

namespace deskflow::filetransfer {

enum class FileTransferEdgeFrameError
{
  None,
  TruncatedLength,
  FrameTooLarge,
  TruncatedPayload,
  InvalidPayload,
};

struct FileTransferEdgeFrameReadResult
{
  FileTransferEdgeFrameError error = FileTransferEdgeFrameError::None;
  FileTransferEdgeCodecError codecError = FileTransferEdgeCodecError::None;
  std::optional<FileTransferEdgeMessage> message;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferEdgeFrameError::None;
  }
};

[[nodiscard]] FileTransferEdgeEncodeResult writeFileTransferEdgeFrame(
    IStream *stream, const FileTransferEdgeMessage &message, const FileTransferEdgeLimits &limits = {}
);

[[nodiscard]] FileTransferEdgeFrameReadResult
readFileTransferEdgeFrame(IStream *stream, const FileTransferEdgeLimits &limits = {});

} // namespace deskflow::filetransfer
