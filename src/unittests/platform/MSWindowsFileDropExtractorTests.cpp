/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsFileDropExtractor.h"

#include <ShlObj_core.h>
#include <Windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using deskflow::filetransfer::extractWindowsFileDrop;
using deskflow::filetransfer::WindowsFileDropError;
using deskflow::filetransfer::WindowsFileDropLimits;

class DropBlock
{
public:
  explicit DropBlock(const std::vector<std::wstring> &paths)
  {
    std::wstring payload;
    for (const auto &path : paths) {
      payload.append(path);
      payload.push_back(L'\0');
    }

    payload.push_back(L'\0');
    if (paths.empty()) {
      payload.push_back(L'\0');
    }

    const auto byteCount = sizeof(DROPFILES) + payload.size() * sizeof(wchar_t);
    m_handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, byteCount);
    if (m_handle == nullptr) {
      throw std::runtime_error("GlobalAlloc failed");
    }

    auto *memory = GlobalLock(m_handle);
    if (memory == nullptr) {
      GlobalFree(m_handle);
      m_handle = nullptr;
      throw std::runtime_error("GlobalLock failed");
    }

    auto *header = static_cast<DROPFILES *>(memory);
    header->pFiles = static_cast<DWORD>(sizeof(DROPFILES));
    header->fWide = TRUE;

    auto *pathBytes = static_cast<unsigned char *>(memory) + sizeof(DROPFILES);
    std::memcpy(pathBytes, payload.data(), payload.size() * sizeof(wchar_t));
    GlobalUnlock(m_handle);
  }

  DropBlock(const DropBlock &) = delete;
  DropBlock &operator=(const DropBlock &) = delete;

  ~DropBlock()
  {
    if (m_handle != nullptr) {
      GlobalFree(m_handle);
    }
  }

  [[nodiscard]] HDROP get() const noexcept
  {
    return reinterpret_cast<HDROP>(m_handle);
  }

private:
  HGLOBAL m_handle = nullptr;
};

bool expect(bool condition, const std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
  }
  return condition;
}

bool rejectsNullAndEmptyDrops()
{
  const auto nullResult = extractWindowsFileDrop(nullptr);
  if (!expect(nullResult.error == WindowsFileDropError::NullHandle, "null handle error")) {
    return false;
  }
  if (!expect(nullResult.paths.empty(), "null handle result is atomic")) {
    return false;
  }

  const DropBlock empty({});
  const auto emptyResult = extractWindowsFileDrop(empty.get());
  return expect(emptyResult.error == WindowsFileDropError::EmptyDrop, "empty drop error") &&
         expect(emptyResult.paths.empty(), "empty drop has no paths");
}

bool extractsOneUnicodePath()
{
  const std::wstring path = L"C:\\전송\\보고서-😀.txt";
  const DropBlock drop({path});

  const auto result = extractWindowsFileDrop(drop.get());
  return expect(result.ok(), "Unicode drop succeeds") && expect(result.paths.size() == 1, "one path returned") &&
         expect(result.paths.front().native() == path, "Unicode path is preserved exactly");
}

bool preservesMultiplePathOrder()
{
  const std::vector<std::wstring> paths = {
      L"C:\\first.txt",
      L"D:\\폴더\\second.bin",
      L"E:\\third file.dat",
  };
  const DropBlock drop(paths);

  const auto result = extractWindowsFileDrop(drop.get());
  if (!expect(result.ok(), "multi-file drop succeeds") ||
      !expect(result.paths.size() == paths.size(), "all paths returned")) {
    return false;
  }

  for (std::size_t index = 0; index < paths.size(); ++index) {
    if (!expect(result.paths[index].native() == paths[index], "path order is stable")) {
      return false;
    }
  }
  return true;
}

bool supportsPathsBeyondLegacyMaxPath()
{
  const std::wstring path = L"C:\\" + std::wstring(300, L'a') + L".txt";
  const DropBlock drop({path});

  const auto result = extractWindowsFileDrop(drop.get());
  return expect(result.ok(), "long native path succeeds") &&
         expect(result.paths.front().native() == path, "long path is not truncated");
}

bool honorsItemLimitBoundary()
{
  const DropBlock drop({L"C:\\one.txt", L"C:\\two.txt"});

  const WindowsFileDropLimits exactLimit{.maxItems = 2};
  if (!expect(extractWindowsFileDrop(drop.get(), exactLimit).ok(), "exact item limit succeeds")) {
    return false;
  }

  const WindowsFileDropLimits lowLimit{.maxItems = 1};
  const auto rejected = extractWindowsFileDrop(drop.get(), lowLimit);
  return expect(rejected.error == WindowsFileDropError::TooManyItems, "item count over limit is rejected") &&
         expect(rejected.paths.empty(), "item-limit rejection is atomic");
}

bool honorsPathLimitBoundary()
{
  const std::wstring path = L"C:\\boundary.txt";
  const DropBlock drop({path});

  const WindowsFileDropLimits exactLimit{.maxPathCharacters = path.size()};
  if (!expect(extractWindowsFileDrop(drop.get(), exactLimit).ok(), "exact path limit succeeds")) {
    return false;
  }

  const WindowsFileDropLimits lowLimit{.maxPathCharacters = path.size() - 1};
  const auto rejected = extractWindowsFileDrop(drop.get(), lowLimit);
  return expect(rejected.error == WindowsFileDropError::PathTooLong, "path over limit is rejected") &&
         expect(rejected.itemPosition == 0, "rejected path position is reported") &&
         expect(rejected.paths.empty(), "path-limit rejection is atomic");
}

bool discardsEarlierPathsWhenALaterPathFails()
{
  const std::wstring shortPath = L"C:\\ok.txt";
  const std::wstring longPath = L"C:\\" + std::wstring(40, L'x') + L".txt";
  const DropBlock drop({shortPath, longPath});
  const WindowsFileDropLimits limits{.maxPathCharacters = shortPath.size()};

  const auto result = extractWindowsFileDrop(drop.get(), limits);
  return expect(result.error == WindowsFileDropError::PathTooLong, "later invalid path is rejected") &&
         expect(result.itemPosition == 1, "later invalid path position is reported") &&
         expect(result.paths.empty(), "partial extraction is discarded");
}

struct TestCase
{
  const char *name;
  bool (*run)();
};

} // namespace

int main()
{
  const TestCase tests[] = {
      {"rejectsNullAndEmptyDrops", rejectsNullAndEmptyDrops},
      {"extractsOneUnicodePath", extractsOneUnicodePath},
      {"preservesMultiplePathOrder", preservesMultiplePathOrder},
      {"supportsPathsBeyondLegacyMaxPath", supportsPathsBeyondLegacyMaxPath},
      {"honorsItemLimitBoundary", honorsItemLimitBoundary},
      {"honorsPathLimitBoundary", honorsPathLimitBoundary},
      {"discardsEarlierPathsWhenALaterPathFails", discardsEarlierPathsWhenALaterPathFails},
  };

  int failures = 0;
  for (const auto &test : tests) {
    try {
      if (test.run()) {
        std::cout << "PASS: " << test.name << '\n';
      } else {
        ++failures;
      }
    } catch (const std::exception &error) {
      std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
      ++failures;
    }
  }

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
