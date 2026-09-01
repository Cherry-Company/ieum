/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsFileTransferPlatform.h"

#include "platform/MSWindowsFileTransferIo.h"
#include "platform/MSWindowsFileTransferSource.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <system_error>
#include <utility>

namespace deskflow::filetransfer {
namespace {

FileTransferIoError portableSourceError(MSWindowsFileTransferSourceError error)
{
  switch (error) {
  case MSWindowsFileTransferSourceError::None:
    return FileTransferIoError::None;
  case MSWindowsFileTransferSourceError::EmptyPaths:
    return FileTransferIoError::EmptyPaths;
  case MSWindowsFileTransferSourceError::TooManyPaths:
    return FileTransferIoError::TooManyPaths;
  case MSWindowsFileTransferSourceError::EmptyPath:
    return FileTransferIoError::EmptyPath;
  case MSWindowsFileTransferSourceError::RelativePath:
    return FileTransferIoError::RelativePath;
  case MSWindowsFileTransferSourceError::OpenFailed:
    return FileTransferIoError::OpenFailed;
  case MSWindowsFileTransferSourceError::MetadataReadFailed:
    return FileTransferIoError::MetadataReadFailed;
  case MSWindowsFileTransferSourceError::ReparsePoint:
  case MSWindowsFileTransferSourceError::NotRegularFile:
    return FileTransferIoError::LinkOrSpecialFile;
  case MSWindowsFileTransferSourceError::FilenameConversionFailed:
    return FileTransferIoError::FilenameConversionFailed;
  }
  return FileTransferIoError::MetadataReadFailed;
}

FileTransferIoError portableIoError(MSWindowsFileTransferIoError error)
{
  switch (error) {
  case MSWindowsFileTransferIoError::None:
    return FileTransferIoError::None;
  case MSWindowsFileTransferIoError::InvalidSource:
  case MSWindowsFileTransferIoError::SourceChanged:
    return FileTransferIoError::SourceChanged;
  case MSWindowsFileTransferIoError::OpenFailed:
    return FileTransferIoError::OpenFailed;
  case MSWindowsFileTransferIoError::ReadFailed:
    return FileTransferIoError::ReadFailed;
  case MSWindowsFileTransferIoError::InvalidOffer:
    return FileTransferIoError::InvalidOffer;
  case MSWindowsFileTransferIoError::InvalidDestination:
    return FileTransferIoError::InvalidDestination;
  case MSWindowsFileTransferIoError::DestinationDenied:
    return FileTransferIoError::DestinationDenied;
  case MSWindowsFileTransferIoError::WrongPhase:
    return FileTransferIoError::WrongPhase;
  case MSWindowsFileTransferIoError::RouteMismatch:
    return FileTransferIoError::RouteMismatch;
  case MSWindowsFileTransferIoError::WrongItem:
    return FileTransferIoError::WrongItem;
  case MSWindowsFileTransferIoError::WrongOffset:
    return FileTransferIoError::WrongOffset;
  case MSWindowsFileTransferIoError::SizeExceeded:
    return FileTransferIoError::SizeExceeded;
  case MSWindowsFileTransferIoError::WriteFailed:
    return FileTransferIoError::WriteFailed;
  case MSWindowsFileTransferIoError::HashFailed:
    return FileTransferIoError::HashFailed;
  case MSWindowsFileTransferIoError::IntegrityMismatch:
    return FileTransferIoError::IntegrityMismatch;
  case MSWindowsFileTransferIoError::PublishFailed:
    return FileTransferIoError::PublishFailed;
  }
  return FileTransferIoError::WriteFailed;
}

class Reader final : public IFileTransferReader
{
public:
  explicit Reader(std::unique_ptr<MSWindowsFileTransferReader> reader) : m_reader(std::move(reader))
  {
  }

  FileTransferReadResult readNext() override
  {
    auto result = m_reader->readNext();
    return {
        .error = portableIoError(result.error),
        .offset = result.offset,
        .bytes = std::move(result.bytes),
        .complete = result.complete,
        .sha256 = result.sha256,
    };
  }

private:
  std::unique_ptr<MSWindowsFileTransferReader> m_reader;
};

FileTransferWriterMutation portableMutation(MSWindowsFileTransferWriterMutation mutation)
{
  return {
      .error = portableIoError(mutation.error),
      .publishedPaths = std::move(mutation.publishedPaths),
  };
}

class Writer final : public IFileTransferWriter
{
public:
  explicit Writer(std::unique_ptr<MSWindowsFileTransferWriter> writer) : m_writer(std::move(writer))
  {
  }

  FileTransferWriterMutation begin(const FileTransferDataBegin &message) override
  {
    return portableMutation(m_writer->begin(message));
  }

  FileTransferWriterMutation writeChunk(const FileTransferDataChunk &message) override
  {
    return portableMutation(m_writer->writeChunk(message));
  }

  FileTransferWriterMutation endItem(const FileTransferDataItemEnd &message) override
  {
    return portableMutation(m_writer->endItem(message));
  }

  FileTransferWriterMutation finish(const FileTransferDataFinish &message) override
  {
    return portableMutation(m_writer->finish(message));
  }

private:
  std::unique_ptr<MSWindowsFileTransferWriter> m_writer;
};

} // namespace

FileTransferSourceInspection
MSWindowsFileTransferPlatform::inspectSources(const std::vector<std::filesystem::path> &paths, std::size_t maxItems)
{
  auto result = inspectMSWindowsFileTransferSources(paths, maxItems);
  return {
      .error = portableSourceError(result.error),
      .itemPosition = result.itemPosition,
      .sources = std::move(result.sources),
  };
}

FileTransferReaderOpenResult
MSWindowsFileTransferPlatform::openReader(FileTransferSourceCandidate source, std::size_t chunkBytes)
{
  auto result = MSWindowsFileTransferReader::open(std::move(source), chunkBytes);
  if (!result.ok()) {
    return {.error = portableIoError(result.error)};
  }
  return {.reader = std::make_unique<Reader>(std::move(result.reader))};
}

FileTransferWriterCreateResult
MSWindowsFileTransferPlatform::createWriter(std::filesystem::path destination, FileTransferOffer offer)
{
  auto result = MSWindowsFileTransferWriter::create(destination, offer);
  if (result.error == MSWindowsFileTransferIoError::InvalidDestination && !destination.empty() &&
      destination.is_absolute()) {
    std::error_code error;
    std::filesystem::create_directories(destination, error);
    if (error) {
      return {.error = FileTransferIoError::DestinationDenied};
    }
    result = MSWindowsFileTransferWriter::create(destination, std::move(offer));
  }
  if (!result.ok()) {
    return {.error = portableIoError(result.error)};
  }
  return {.writer = std::make_unique<Writer>(std::move(result.writer))};
}

std::optional<TransferId> MSWindowsFileTransferPlatform::randomId()
{
  TransferId id{};
  const auto status =
      BCryptGenRandom(nullptr, id.data(), static_cast<ULONG>(id.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (status != 0 || std::ranges::all_of(id, [](std::uint8_t value) { return value == 0; })) {
    return std::nullopt;
  }
  return id;
}

} // namespace deskflow::filetransfer
