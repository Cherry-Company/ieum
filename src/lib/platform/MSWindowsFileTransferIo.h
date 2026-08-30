/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferDataCodec.h"
#include "deskflow/FileTransferSourceManifest.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace deskflow::filetransfer {

enum class MSWindowsFileTransferIoError
{
  None,
  InvalidSource,
  OpenFailed,
  SourceChanged,
  ReadFailed,
  InvalidOffer,
  InvalidDestination,
  DestinationDenied,
  WrongPhase,
  RouteMismatch,
  WrongItem,
  WrongOffset,
  SizeExceeded,
  WriteFailed,
  HashFailed,
  IntegrityMismatch,
  PublishFailed,
};

struct MSWindowsFileTransferReadResult
{
  MSWindowsFileTransferIoError error = MSWindowsFileTransferIoError::None;
  std::uint64_t offset = 0;
  std::vector<std::uint8_t> bytes;
  bool complete = false;
  FileTransferDigest sha256{};

  [[nodiscard]] bool ok() const noexcept
  {
    return error == MSWindowsFileTransferIoError::None;
  }
};

class MSWindowsFileTransferReader;

struct MSWindowsFileTransferReaderOpenResult
{
  MSWindowsFileTransferIoError error = MSWindowsFileTransferIoError::None;
  std::unique_ptr<MSWindowsFileTransferReader> reader;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == MSWindowsFileTransferIoError::None;
  }
};

class MSWindowsFileTransferReader final
{
public:
  static MSWindowsFileTransferReaderOpenResult
  open(FileTransferSourceCandidate source, std::size_t chunkBytes = 60 * 1024);

  ~MSWindowsFileTransferReader();

  MSWindowsFileTransferReader(const MSWindowsFileTransferReader &) = delete;
  MSWindowsFileTransferReader &operator=(const MSWindowsFileTransferReader &) = delete;

  [[nodiscard]] MSWindowsFileTransferReadResult readNext();

private:
  struct Impl;
  explicit MSWindowsFileTransferReader(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> m_impl;
};

struct MSWindowsFileTransferWriterMutation
{
  MSWindowsFileTransferIoError error = MSWindowsFileTransferIoError::None;
  std::vector<std::filesystem::path> publishedPaths;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == MSWindowsFileTransferIoError::None;
  }
};

class MSWindowsFileTransferWriter;

struct MSWindowsFileTransferWriterCreateResult
{
  MSWindowsFileTransferIoError error = MSWindowsFileTransferIoError::None;
  std::unique_ptr<MSWindowsFileTransferWriter> writer;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == MSWindowsFileTransferIoError::None;
  }
};

class MSWindowsFileTransferWriter final
{
public:
  static MSWindowsFileTransferWriterCreateResult
  create(std::filesystem::path destinationDirectory, FileTransferOffer offer);

  ~MSWindowsFileTransferWriter();

  MSWindowsFileTransferWriter(const MSWindowsFileTransferWriter &) = delete;
  MSWindowsFileTransferWriter &operator=(const MSWindowsFileTransferWriter &) = delete;

  [[nodiscard]] MSWindowsFileTransferWriterMutation begin(const FileTransferDataBegin &message);
  [[nodiscard]] MSWindowsFileTransferWriterMutation writeChunk(const FileTransferDataChunk &message);
  [[nodiscard]] MSWindowsFileTransferWriterMutation endItem(const FileTransferDataItemEnd &message);
  [[nodiscard]] MSWindowsFileTransferWriterMutation finish(const FileTransferDataFinish &message);

private:
  struct Impl;
  explicit MSWindowsFileTransferWriter(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::filetransfer
