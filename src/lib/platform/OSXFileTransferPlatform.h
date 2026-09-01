/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferPlatform.h"

namespace deskflow::filetransfer {

class OSXFileTransferPlatform final : public IFileTransferPlatform
{
public:
  [[nodiscard]] FileTransferSourceInspection
  inspectSources(const std::vector<std::filesystem::path> &paths, std::size_t maxItems) override;
  [[nodiscard]] FileTransferReaderOpenResult
  openReader(FileTransferSourceCandidate source, std::size_t chunkBytes) override;
  [[nodiscard]] FileTransferWriterCreateResult
  createWriter(std::filesystem::path destination, FileTransferOffer offer) override;
  [[nodiscard]] std::optional<TransferId> randomId() override;
};

} // namespace deskflow::filetransfer
