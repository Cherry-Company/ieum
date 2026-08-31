/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsFileTransferPlatform.h"
#include "platform/MSWindowsFileTransferSource.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
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

void inspectsUnicodeRegularFilesInOrder()
{
  TemporaryDirectory directory;
  const auto first = directory.path() / L"한글 문서.txt";
  const auto second = directory.path() / L"zero.bin";
  writeFile(first, "payload");
  writeFile(second, "");

  const auto result = inspectMSWindowsFileTransferSources({first, second});

  require(result.ok(), "two valid files should be inspected");
  require(result.error == MSWindowsFileTransferSourceError::None, "success error should be None");
  require(result.sources.size() == 2, "both files should be returned");
  require(result.sources[0].path == first, "first path order should be preserved");
  require(result.sources[1].path == second, "second path order should be preserved");
  require(result.sources[0].name == "한글 문서.txt", "Unicode basename should be converted to UTF-8");
  require(result.sources[1].name == "zero.bin", "ASCII basename should be preserved");
  require(result.sources[0].size == 7, "first file size should be captured");
  require(result.sources[1].size == 0, "zero-byte file should be captured");
  require(result.sources[0].identity.length > 0, "file identity should be captured");
  require(result.sources[0].lastWriteTime > 0, "last-write time should be captured");
}

void rejectsListsAndPathsBeforeReturningPartialData()
{
  auto result = inspectMSWindowsFileTransferSources({});
  require(result.error == MSWindowsFileTransferSourceError::EmptyPaths, "empty path list should fail");
  require(!result.itemPosition.has_value(), "empty list should not have an item position");

  result = inspectMSWindowsFileTransferSources({"first", "second"}, 1);
  require(result.error == MSWindowsFileTransferSourceError::TooManyPaths, "oversized list should fail first");
  require(result.sources.empty(), "oversized list should not inspect any source");

  result = inspectMSWindowsFileTransferSources({"relative.txt"});
  require(result.error == MSWindowsFileTransferSourceError::RelativePath, "relative path should fail");
  require(result.itemPosition == 0, "relative path should identify its position");

  TemporaryDirectory directory;
  const auto missing = directory.path() / L"missing.txt";
  result = inspectMSWindowsFileTransferSources({missing});
  require(result.error == MSWindowsFileTransferSourceError::OpenFailed, "missing path should fail to open");
  require(result.itemPosition == 0, "missing path should identify its position");
  require(result.sources.empty(), "failed inspection should be atomic");

  result = inspectMSWindowsFileTransferSources({directory.path()});
  require(result.error == MSWindowsFileTransferSourceError::NotRegularFile, "directory should not be a source file");
  require(result.itemPosition == 0, "directory should identify its position");
}

void capturesStableIdentityAndUpdatedSize()
{
  TemporaryDirectory directory;
  const auto path = directory.path() / L"snapshot.bin";
  writeFile(path, "one");
  const auto first = inspectMSWindowsFileTransferSources({path});
  require(first.ok(), "first snapshot should succeed");

  writeFile(path, "a larger payload");
  const auto second = inspectMSWindowsFileTransferSources({path});
  require(second.ok(), "second snapshot should succeed");
  require(first.sources[0].identity == second.sources[0].identity, "same file should retain its identity");
  require(first.sources[0].size == 3, "first snapshot should keep its original size");
  require(second.sources[0].size == 16, "second snapshot should see the changed size");
}

void exposesPortableInspectionAndRandomIds()
{
  TemporaryDirectory directory;
  const auto path = directory.path() / L"portable.txt";
  writeFile(path, "adapter");

  MSWindowsFileTransferPlatform platform;
  const auto result = platform.inspectSources({path}, 100);

  require(result.ok(), "portable platform should inspect an ordinary file");
  require(result.error == FileTransferIoError::None, "portable success should report no error");
  require(result.sources.size() == 1 && result.sources.front().name == "portable.txt", "portable source should match");
  const auto id = platform.randomId();
  require(id.has_value(), "Windows platform should generate a transfer ID");
  require(
      !std::ranges::all_of(*id, [](std::uint8_t value) { return value == 0; }),
      "Windows platform transfer ID must not be all zero"
  );
}

} // namespace

int main()
{
  inspectsUnicodeRegularFilesInOrder();
  rejectsListsAndPathsBeforeReturningPartialData();
  capturesStableIdentityAndUpdatedSize();
  exposesPortableInspectionAndRandomIds();
  std::cout << "PASS: MSWindowsFileTransferSourceTests\n";
  return 0;
}
