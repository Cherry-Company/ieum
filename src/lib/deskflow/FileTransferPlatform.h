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
#include <optional>
#include <vector>

namespace deskflow::filetransfer {

enum class FileTransferIoError
{
  None,
  EmptyPaths,
  TooManyPaths,
  EmptyPath,
  RelativePath,
  OpenFailed,
  MetadataReadFailed,
  LinkOrSpecialFile,
  FilenameConversionFailed,
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
  RandomUnavailable,
};

struct FileTransferSourceInspection
{
  FileTransferIoError error = FileTransferIoError::None;
  std::optional<std::size_t> itemPosition;
  std::vector<FileTransferSourceCandidate> sources;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferIoError::None;
  }
};

struct FileTransferReadResult
{
  FileTransferIoError error = FileTransferIoError::None;
  std::uint64_t offset = 0;
  std::vector<std::uint8_t> bytes;
  bool complete = false;
  FileTransferDigest sha256{};

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferIoError::None;
  }
};

class IFileTransferReader
{
public:
  virtual ~IFileTransferReader() = default;
  [[nodiscard]] virtual FileTransferReadResult readNext() = 0;
};

struct FileTransferReaderOpenResult
{
  FileTransferIoError error = FileTransferIoError::None;
  std::unique_ptr<IFileTransferReader> reader;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferIoError::None && reader != nullptr;
  }
};

struct FileTransferWriterMutation
{
  FileTransferIoError error = FileTransferIoError::None;
  std::vector<std::filesystem::path> publishedPaths;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferIoError::None;
  }
};

class IFileTransferWriter
{
public:
  virtual ~IFileTransferWriter() = default;
  [[nodiscard]] virtual FileTransferWriterMutation begin(const FileTransferDataBegin &) = 0;
  [[nodiscard]] virtual FileTransferWriterMutation writeChunk(const FileTransferDataChunk &) = 0;
  [[nodiscard]] virtual FileTransferWriterMutation endItem(const FileTransferDataItemEnd &) = 0;
  [[nodiscard]] virtual FileTransferWriterMutation finish(const FileTransferDataFinish &) = 0;
};

struct FileTransferWriterCreateResult
{
  FileTransferIoError error = FileTransferIoError::None;
  std::unique_ptr<IFileTransferWriter> writer;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferIoError::None && writer != nullptr;
  }
};

class IFileTransferPlatform
{
public:
  virtual ~IFileTransferPlatform() = default;

  [[nodiscard]] virtual FileTransferSourceInspection
  inspectSources(const std::vector<std::filesystem::path> &, std::size_t maxItems) = 0;
  [[nodiscard]] virtual FileTransferReaderOpenResult
  openReader(FileTransferSourceCandidate, std::size_t chunkBytes) = 0;
  [[nodiscard]] virtual FileTransferWriterCreateResult createWriter(std::filesystem::path, FileTransferOffer) = 0;
  [[nodiscard]] virtual std::optional<TransferId> randomId() = 0;
};

} // namespace deskflow::filetransfer
