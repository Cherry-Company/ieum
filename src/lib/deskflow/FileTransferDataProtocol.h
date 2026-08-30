/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferDataCodec.h"

#include <optional>

namespace deskflow {
class IStream;
}

namespace deskflow::filetransfer {

enum class FileTransferDataFrameError
{
  None,
  TruncatedLength,
  FrameTooLarge,
  TruncatedPayload,
  InvalidPayload,
};

struct FileTransferDataFrameReadResult
{
  FileTransferDataFrameError error = FileTransferDataFrameError::None;
  FileTransferDataCodecError codecError = FileTransferDataCodecError::None;
  std::optional<FileTransferDataMessage> message;
  OfferValidation validation;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferDataFrameError::None;
  }
};

[[nodiscard]] FileTransferDataEncodeResult writeFileTransferDataFrame(
    IStream *stream, const FileTransferDataMessage &message, const FileTransferDataLimits &limits = {}
);

[[nodiscard]] FileTransferDataFrameReadResult
readFileTransferDataFrame(IStream *stream, const FileTransferDataLimits &limits = {});

} // namespace deskflow::filetransfer
