/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsFileTransferIo.h"

#include "platform/MSWindowsFileTransferSource.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace deskflow::filetransfer;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

class TemporaryDirectory final
{
public:
  TemporaryDirectory()
  {
    std::wstring buffer(MAX_PATH, L'\0');
    const auto length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (length == 0 || length >= buffer.size()) {
      throw std::runtime_error("GetTempPathW failed");
    }
    buffer.resize(length);

    std::wstring unique(MAX_PATH, L'\0');
    if (GetTempFileNameW(buffer.c_str(), L"ium", 0, unique.data()) == 0) {
      throw std::runtime_error("GetTempFileNameW failed");
    }
    unique.resize(std::char_traits<wchar_t>::length(unique.c_str()));
    if (DeleteFileW(unique.c_str()) == 0 || CreateDirectoryW(unique.c_str(), nullptr) == 0) {
      throw std::runtime_error("temporary directory creation failed");
    }
    m_path = std::filesystem::path(std::move(unique));
  }

  ~TemporaryDirectory()
  {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
  }

  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

  [[nodiscard]] const std::filesystem::path &path() const noexcept
  {
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

class FileHandle final
{
public:
  explicit FileHandle(HANDLE handle) : m_handle(handle)
  {
  }

  ~FileHandle()
  {
    if (m_handle != INVALID_HANDLE_VALUE) {
      CloseHandle(m_handle);
    }
  }

  FileHandle(const FileHandle &) = delete;
  FileHandle &operator=(const FileHandle &) = delete;

  [[nodiscard]] HANDLE get() const noexcept
  {
    return m_handle;
  }

private:
  HANDLE m_handle = INVALID_HANDLE_VALUE;
};

void writeFile(const std::filesystem::path &path, std::string_view bytes)
{
  FileHandle file(CreateFileW(
      path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL, nullptr
  ));
  if (file.get() == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("CreateFileW failed");
  }

  DWORD written = 0;
  if (!bytes.empty() &&
      (WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) == 0 ||
       written != bytes.size())) {
    throw std::runtime_error("WriteFile failed");
  }
}

std::string readFile(const std::filesystem::path &path)
{
  FileHandle file(CreateFileW(
      path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, nullptr
  ));
  if (file.get() == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("CreateFileW for read failed");
  }

  LARGE_INTEGER size{};
  if (GetFileSizeEx(file.get(), &size) == 0 || size.QuadPart < 0 || size.QuadPart > 1024 * 1024) {
    throw std::runtime_error("GetFileSizeEx failed");
  }
  std::string result(static_cast<std::size_t>(size.QuadPart), '\0');
  DWORD read = 0;
  if (!result.empty() && (ReadFile(file.get(), result.data(), static_cast<DWORD>(result.size()), &read, nullptr) == 0 ||
                          read != result.size())) {
    throw std::runtime_error("ReadFile failed");
  }
  return result;
}

TransferId transferId(std::uint8_t first = 0x42)
{
  TransferId id{};
  id[0] = first;
  return id;
}

FileTransferDataRoute routeFor(const FileTransferOffer &offer)
{
  return {.id = offer.id, .sourceScreen = offer.sourceScreen, .targetScreen = offer.targetScreen};
}

FileTransferOffer offerWith(std::string name, std::uint64_t size, std::uint8_t id = 0x42)
{
  return {
      .id = transferId(id),
      .sourceScreen = "source",
      .targetScreen = "target",
      .items = {{0, std::move(name), size}},
  };
}

FileTransferDigest digestFromHex(std::string_view hex)
{
  require(hex.size() == 64, "digest fixture must contain 64 hex characters");
  const auto nibble = [](char value) -> std::uint8_t {
    if (value >= '0' && value <= '9') {
      return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
      return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    return 0xff;
  };

  FileTransferDigest digest{};
  for (std::size_t index = 0; index < digest.size(); ++index) {
    const auto high = nibble(hex[index * 2]);
    const auto low = nibble(hex[index * 2 + 1]);
    require(high != 0xff && low != 0xff, "digest fixture must be lowercase hexadecimal");
    digest[index] = static_cast<std::uint8_t>((high << 4U) | low);
  }
  return digest;
}

FileTransferSourceCandidate inspectOne(const std::filesystem::path &path)
{
  auto inspected = inspectMSWindowsFileTransferSources({path});
  require(inspected.ok() && inspected.sources.size() == 1, "source fixture should inspect successfully");
  return std::move(inspected.sources.front());
}

bool hasStagingEntry(const std::filesystem::path &directory)
{
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().filename().native().starts_with(L".ieum-")) {
      return true;
    }
  }
  return false;
}

void readerStreamsSequentialChunksAndHashesTheSnapshot()
{
  TemporaryDirectory directory;
  const auto path = directory.path() / L"한글 원본.txt";
  writeFile(path, "abc");

  auto opened = MSWindowsFileTransferReader::open(inspectOne(path), 2);
  require(opened.ok(), "stable source should open for transfer");

  auto first = opened.reader->readNext();
  require(first.ok() && first.offset == 0, "first source chunk should start at zero");
  require(first.bytes == std::vector<std::uint8_t>({'a', 'b'}), "first source chunk should preserve bytes");
  require(!first.complete, "first partial chunk should not complete the item");

  auto second = opened.reader->readNext();
  require(second.ok() && second.offset == 2, "second source chunk should continue sequentially");
  require(second.bytes == std::vector<std::uint8_t>({'c'}), "second source chunk should preserve bytes");
  require(second.complete, "last source chunk should complete the item");
  require(
      second.sha256 == digestFromHex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
      "reader should compute SHA-256 for the exact streamed bytes"
  );

  require(
      opened.reader->readNext().error == MSWindowsFileTransferIoError::WrongPhase,
      "completed reader should reject an extra read"
  );
}

void readerHandlesEmptyFilesAndRejectsChangedSources()
{
  TemporaryDirectory directory;
  const auto emptyPath = directory.path() / L"empty.bin";
  writeFile(emptyPath, "");
  auto opened = MSWindowsFileTransferReader::open(inspectOne(emptyPath), 1024);
  require(opened.ok(), "empty source should open");
  const auto empty = opened.reader->readNext();
  require(
      empty.ok() && empty.complete && empty.bytes.empty(), "empty source should finish without an empty data chunk"
  );
  require(
      empty.sha256 == digestFromHex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
      "empty source should use the standard SHA-256 digest"
  );

  const auto changedPath = directory.path() / L"changed.bin";
  writeFile(changedPath, "old");
  auto snapshot = inspectOne(changedPath);
  writeFile(changedPath, "a different payload");
  opened = MSWindowsFileTransferReader::open(std::move(snapshot), 1024);
  require(opened.error == MSWindowsFileTransferIoError::SourceChanged, "changed source should fail before streaming");
  require(opened.reader == nullptr, "changed source should not leak a usable reader");
}

void writerPublishesAtomicallyWithoutOverwritingExistingFiles()
{
  TemporaryDirectory directory;
  const auto existing = directory.path() / L"report.txt";
  writeFile(existing, "keep");

  const auto offer = offerWith("report.txt", 3);
  auto created = MSWindowsFileTransferWriter::create(directory.path(), offer);
  require(created.ok(), "valid destination and offer should create a receiver");
  const auto route = routeFor(offer);
  require(created.writer->begin(FileTransferDataBegin{route}).ok(), "matching begin should start receiver");
  require(
      created.writer->writeChunk(FileTransferDataChunk{.route = route, .itemIndex = 0, .offset = 0, .bytes = {'a', 'b'}}
      ).ok(),
      "first sequential chunk should write"
  );
  require(
      created.writer->writeChunk(FileTransferDataChunk{.route = route, .itemIndex = 0, .offset = 2, .bytes = {'c'}}
      ).ok(),
      "last sequential chunk should write"
  );
  require(
      created.writer
          ->endItem(FileTransferDataItemEnd{
              .route = route,
              .itemIndex = 0,
              .size = 3,
              .sha256 = digestFromHex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
          })
          .ok(),
      "matching item digest should seal the staged file"
  );

  const auto finished = created.writer->finish(FileTransferDataFinish{.route = route, .itemCount = 1, .totalBytes = 3});
  require(finished.ok() && finished.publishedPaths.size() == 1, "finish should publish exactly one file");
  require(readFile(existing) == "keep", "existing destination file must never be overwritten");
  require(finished.publishedPaths[0].filename() == L"report (1).txt", "collision should select a unique final name");
  require(readFile(finished.publishedPaths[0]) == "abc", "published file should contain verified bytes");
  require(!hasStagingEntry(directory.path()), "successful publish should remove its staging directory");
}

void writerSupportsVerifiedZeroByteFiles()
{
  TemporaryDirectory directory;
  const auto offer = offerWith("empty.bin", 0, 0x43);
  const auto route = routeFor(offer);
  auto created = MSWindowsFileTransferWriter::create(directory.path(), offer);
  require(created.ok() && created.writer->begin(FileTransferDataBegin{route}).ok(), "zero-byte receive should begin");
  require(
      created.writer
          ->endItem(FileTransferDataItemEnd{
              .route = route,
              .itemIndex = 0,
              .size = 0,
              .sha256 = digestFromHex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
          })
          .ok(),
      "zero-byte item should seal without a chunk"
  );
  const auto finished = created.writer->finish(FileTransferDataFinish{.route = route, .itemCount = 1, .totalBytes = 0});
  require(finished.ok() && finished.publishedPaths.size() == 1, "zero-byte item should publish");
  require(readFile(finished.publishedPaths[0]).empty(), "published zero-byte file should stay empty");
}

void writerFailsClosedOnSequenceAndIntegrityErrors()
{
  TemporaryDirectory directory;
  const auto offer = offerWith("failed.bin", 3, 0x44);
  const auto route = routeFor(offer);

  auto created = MSWindowsFileTransferWriter::create(directory.path(), offer);
  require(created.ok() && created.writer->begin(FileTransferDataBegin{route}).ok(), "sequence test should begin");
  const auto wrongOffset =
      created.writer->writeChunk(FileTransferDataChunk{.route = route, .itemIndex = 0, .offset = 1, .bytes = {'x'}});
  require(wrongOffset.error == MSWindowsFileTransferIoError::WrongOffset, "non-sequential offset should abort");
  require(!hasStagingEntry(directory.path()), "sequence failure should remove staged data immediately");
  require(!std::filesystem::exists(directory.path() / L"failed.bin"), "sequence failure should publish nothing");

  created = MSWindowsFileTransferWriter::create(directory.path(), offer);
  require(created.ok() && created.writer->begin(FileTransferDataBegin{route}).ok(), "integrity test should begin");
  require(
      created.writer
          ->writeChunk(FileTransferDataChunk{.route = route, .itemIndex = 0, .offset = 0, .bytes = {'a', 'b', 'c'}})
          .ok(),
      "integrity fixture should write"
  );
  auto wrongDigest = digestFromHex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  wrongDigest[0] ^= 0xff;
  const auto ended =
      created.writer->endItem(FileTransferDataItemEnd{.route = route, .itemIndex = 0, .size = 3, .sha256 = wrongDigest}
      );
  require(ended.error == MSWindowsFileTransferIoError::IntegrityMismatch, "wrong SHA-256 should abort");
  require(!hasStagingEntry(directory.path()), "integrity failure should remove staged data immediately");
  require(!std::filesystem::exists(directory.path() / L"failed.bin"), "integrity failure should publish nothing");
}

void writerRejectsInvalidDestinationOfferAndRoute()
{
  TemporaryDirectory directory;
  auto invalidOffer = offerWith("../escape.txt", 1, 0x45);
  auto created = MSWindowsFileTransferWriter::create(directory.path(), invalidOffer);
  require(created.error == MSWindowsFileTransferIoError::InvalidOffer, "unsafe filename should fail offer policy");

  created = MSWindowsFileTransferWriter::create("relative", offerWith("safe.txt", 1, 0x46));
  require(created.error == MSWindowsFileTransferIoError::InvalidDestination, "relative destination should fail");

  const auto offer = offerWith("safe.txt", 1, 0x47);
  created = MSWindowsFileTransferWriter::create(directory.path(), offer);
  require(created.ok(), "route test writer should create");
  auto wrongRoute = routeFor(offer);
  wrongRoute.targetScreen = "someone-else";
  require(
      created.writer->begin(FileTransferDataBegin{wrongRoute}).error == MSWindowsFileTransferIoError::RouteMismatch,
      "mismatched route should abort before staging"
  );
  require(!hasStagingEntry(directory.path()), "route failure should leave no staging directory");
}

void writerNeverCleansAStagingPathItDidNotCreate()
{
  TemporaryDirectory directory;
  const auto offer = offerWith("safe.txt", 1, 0x48);
  const auto route = routeFor(offer);
  const auto existingStage = directory.path() / L".ieum-48000000000000000000000000000000.part";
  require(std::filesystem::create_directory(existingStage), "staging collision fixture should create");
  const auto existingFile = existingStage / L"0.part";
  writeFile(existingFile, "do not delete");

  auto created = MSWindowsFileTransferWriter::create(directory.path(), offer);
  require(created.ok(), "staging collision writer should create before begin");
  require(
      created.writer->begin(FileTransferDataBegin{route}).error == MSWindowsFileTransferIoError::DestinationDenied,
      "pre-existing staging path should fail closed"
  );
  require(readFile(existingFile) == "do not delete", "receiver must never clean a staging path it did not create");
}

} // namespace

int main()
{
  readerStreamsSequentialChunksAndHashesTheSnapshot();
  readerHandlesEmptyFilesAndRejectsChangedSources();
  writerPublishesAtomicallyWithoutOverwritingExistingFiles();
  writerSupportsVerifiedZeroByteFiles();
  writerFailsClosedOnSequenceAndIntegrityErrors();
  writerRejectsInvalidDestinationOfferAndRoute();
  writerNeverCleansAStagingPathItDidNotCreate();
  std::cout << "PASS: MSWindowsFileTransferIoTests\n";
  return 0;
}
