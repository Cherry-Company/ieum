/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXFileTransferPlatform.h"

#include <QString>

#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
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
    std::string pattern = "/tmp/ieum-file-transfer-XXXXXX";
    if (::mkdtemp(pattern.data()) == nullptr) {
      throw std::runtime_error("mkdtemp failed");
    }
    m_path = pattern;
  }
  ~TemporaryDirectory()
  {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
  }
  [[nodiscard]] const std::filesystem::path &path() const noexcept
  {
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

void writeFile(const std::filesystem::path &path, std::string_view bytes)
{
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!stream) {
    throw std::runtime_error("write fixture failed");
  }
}

std::string readFile(const std::filesystem::path &path)
{
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

TransferId transferId(std::uint8_t first = 0x42)
{
  TransferId id{};
  id[0] = first;
  return id;
}

FileTransferOffer offerWith(std::string name, std::uint64_t size, std::uint8_t id = 0x42)
{
  return {
      .id = transferId(id),
      .sourceScreen = "source",
      .targetScreen = "target",
      .items = {{.index = 0, .name = std::move(name), .size = size}},
  };
}

FileTransferDataRoute routeFor(const FileTransferOffer &offer)
{
  return {.id = offer.id, .sourceScreen = offer.sourceScreen, .targetScreen = offer.targetScreen};
}

FileTransferDigest digestFromHex(std::string_view hex)
{
  const auto nibble = [](char value) -> std::uint8_t {
    if (value >= '0' && value <= '9') {
      return static_cast<std::uint8_t>(value - '0');
    }
    return static_cast<std::uint8_t>(value - 'a' + 10);
  };
  require(hex.size() == 64, "digest fixture length");
  FileTransferDigest digest{};
  for (std::size_t index = 0; index < digest.size(); ++index) {
    digest[index] = static_cast<std::uint8_t>((nibble(hex[index * 2]) << 4U) | nibble(hex[index * 2 + 1]));
  }
  return digest;
}

std::filesystem::path stagingEntry(const std::filesystem::path &directory)
{
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().filename().string().starts_with(".ieum-transfer-")) {
      return entry.path();
    }
  }
  return {};
}

void inspectsRegularNfcFilesAndRejectsLinks()
{
  TemporaryDirectory directory;
  const auto decomposed = QString::fromUtf8("한글 cafe\u0301 file.txt").toUtf8();
  const auto source = directory.path() / std::string(decomposed.constData(), decomposed.size());
  writeFile(source, "abc");

  OSXFileTransferPlatform platform;
  const auto inspected = platform.inspectSources({source}, 100);
  require(inspected.ok() && inspected.sources.size() == 1, "ordinary Unicode source should inspect");
  const auto name = QString::fromUtf8(inspected.sources.front().name);
  require(name == name.normalized(QString::NormalizationForm_C), "source name should be NFC");

  const auto directoryResult = platform.inspectSources({directory.path()}, 100);
  require(directoryResult.error == FileTransferIoError::LinkOrSpecialFile, "directories should be rejected");
  const auto link = directory.path() / "link.txt";
  require(::symlink(source.c_str(), link.c_str()) == 0, "symlink fixture should create");
  const auto linkResult = platform.inspectSources({link}, 100);
  require(linkResult.error == FileTransferIoError::LinkOrSpecialFile, "symlinks should be rejected");
}

void readerStreamsAndRevalidatesTheSnapshot()
{
  TemporaryDirectory directory;
  const auto source = directory.path() / "source.bin";
  writeFile(source, "abc");
  OSXFileTransferPlatform platform;
  auto inspected = platform.inspectSources({source}, 100);
  auto opened = platform.openReader(inspected.sources.front(), 2);
  require(opened.ok(), "stable source should open");
  const auto first = opened.reader->readNext();
  const auto second = opened.reader->readNext();
  require(first.bytes == std::vector<std::uint8_t>({'a', 'b'}) && !first.complete, "first chunk should stream");
  require(second.bytes == std::vector<std::uint8_t>({'c'}) && second.complete, "second chunk should finish");
  require(
      second.sha256 == digestFromHex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
      "reader should hash exact bytes"
  );

  inspected = platform.inspectSources({source}, 100);
  writeFile(source, "replacement");
  opened = platform.openReader(inspected.sources.front(), 2);
  require(opened.error == FileTransferIoError::SourceChanged, "replaced source should fail before reading");
}

void writerPublishesWithoutOverwriteAndCleansStaging()
{
  TemporaryDirectory directory;
  writeFile(directory.path() / "report.txt", "keep");
  const auto offer = offerWith("report.txt", 3);
  const auto route = routeFor(offer);
  OSXFileTransferPlatform platform;
  auto created = platform.createWriter(directory.path(), offer);
  require(created.ok() && created.writer->begin(FileTransferDataBegin{route}).ok(), "writer should begin");

  const auto staging = stagingEntry(directory.path());
  struct stat stagingStatus{};
  require(!staging.empty() && ::stat(staging.c_str(), &stagingStatus) == 0, "staging should exist");
  require((stagingStatus.st_mode & 0777) == 0700, "staging directory should be mode 0700");
  require(
      created.writer
          ->writeChunk(FileTransferDataChunk{.route = route, .itemIndex = 0, .offset = 0, .bytes = {'a', 'b', 'c'}})
          .ok(),
      "sequential bytes should write"
  );
  require(
      created.writer
          ->endItem(
              FileTransferDataItemEnd{
                  .route = route,
                  .itemIndex = 0,
                  .size = 3,
                  .sha256 = digestFromHex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
              }
          )
          .ok(),
      "matching digest should seal item"
  );
  const auto finished = created.writer->finish(FileTransferDataFinish{.route = route, .itemCount = 1, .totalBytes = 3});
  require(finished.ok() && finished.publishedPaths.size() == 1, "finish should publish one file");
  require(readFile(directory.path() / "report.txt") == "keep", "existing file must remain untouched");
  require(finished.publishedPaths.front().filename() == "report (1).txt", "duplicate should get a suffix");
  require(readFile(finished.publishedPaths.front()) == "abc", "published bytes should match");
  require(stagingEntry(directory.path()).empty(), "successful finish should remove staging");
}

void writerRejectsOffsetsDigestsAndCleansOnDestruction()
{
  TemporaryDirectory directory;
  const auto offer = offerWith("failed.bin", 3, 0x43);
  const auto route = routeFor(offer);
  OSXFileTransferPlatform platform;
  auto created = platform.createWriter(directory.path(), offer);
  require(created.ok() && created.writer->begin(FileTransferDataBegin{route}).ok(), "offset fixture should begin");
  require(
      created.writer->writeChunk(FileTransferDataChunk{.route = route, .itemIndex = 0, .offset = 1, .bytes = {'x'}})
              .error == FileTransferIoError::WrongOffset,
      "wrong offset should abort"
  );
  require(stagingEntry(directory.path()).empty(), "offset failure should clean staging");

  created = platform.createWriter(directory.path(), offer);
  require(created.ok() && created.writer->begin(FileTransferDataBegin{route}).ok(), "destructor fixture should begin");
  require(!stagingEntry(directory.path()).empty(), "owned staging should exist before destruction");
  created.writer.reset();
  require(stagingEntry(directory.path()).empty(), "writer destructor should remove only owned staging");
}

} // namespace

int main()
{
  inspectsRegularNfcFilesAndRejectsLinks();
  readerStreamsAndRevalidatesTheSnapshot();
  writerPublishesWithoutOverwriteAndCleansStaging();
  writerRejectsOffsetsDigestsAndCleansOnDestruction();
  std::cout << "PASS: OSXFileTransferPlatformTests\n";
  return 0;
}
