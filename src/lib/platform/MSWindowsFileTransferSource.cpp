/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsFileTransferSource.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace deskflow::filetransfer {
namespace {

class FileHandle final
{
public:
  explicit FileHandle(HANDLE handle) noexcept : m_handle(handle)
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

MSWindowsFileTransferSourceResult failure(MSWindowsFileTransferSourceError error, std::size_t position)
{
  return {.error = error, .itemPosition = position, .sources = {}};
}

std::optional<std::string> utf8Filename(const std::filesystem::path &path)
{
  const auto filename = path.filename().native();
  if (filename.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
    return std::nullopt;
  }
  if (filename.empty()) {
    return std::string{};
  }

  const auto inputLength = static_cast<int>(filename.size());
  const auto outputLength =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, filename.data(), inputLength, nullptr, 0, nullptr, nullptr);
  if (outputLength <= 0) {
    return std::nullopt;
  }

  std::string output(static_cast<std::size_t>(outputLength), '\0');
  if (WideCharToMultiByte(
          CP_UTF8, WC_ERR_INVALID_CHARS, filename.data(), inputLength, output.data(), outputLength, nullptr, nullptr
      ) != outputLength) {
    return std::nullopt;
  }
  return output;
}

std::uint64_t combinedValue(DWORD high, DWORD low) noexcept
{
  return (static_cast<std::uint64_t>(high) << 32U) | low;
}

FileTransferSourceIdentity fileIdentity(const BY_HANDLE_FILE_INFORMATION &information)
{
  FileTransferSourceIdentity identity;
  constexpr auto volumeOffset = std::size_t{0};
  constexpr auto indexHighOffset = sizeof(information.dwVolumeSerialNumber);
  constexpr auto indexLowOffset = indexHighOffset + sizeof(information.nFileIndexHigh);
  identity.length = indexLowOffset + sizeof(information.nFileIndexLow);
  std::memcpy(
      identity.bytes.data() + volumeOffset, &information.dwVolumeSerialNumber, sizeof(information.dwVolumeSerialNumber)
  );
  std::memcpy(identity.bytes.data() + indexHighOffset, &information.nFileIndexHigh, sizeof(information.nFileIndexHigh));
  std::memcpy(identity.bytes.data() + indexLowOffset, &information.nFileIndexLow, sizeof(information.nFileIndexLow));
  return identity;
}

} // namespace

MSWindowsFileTransferSourceResult
inspectMSWindowsFileTransferSources(const std::vector<std::filesystem::path> &paths, std::size_t maxItems)
{
  if (paths.empty()) {
    return {
        .error = MSWindowsFileTransferSourceError::EmptyPaths,
        .itemPosition = std::nullopt,
        .sources = {},
    };
  }
  if (paths.size() > maxItems) {
    return {
        .error = MSWindowsFileTransferSourceError::TooManyPaths,
        .itemPosition = std::nullopt,
        .sources = {},
    };
  }

  std::vector<FileTransferSourceCandidate> sources;
  sources.reserve(paths.size());

  for (std::size_t position = 0; position < paths.size(); ++position) {
    const auto &path = paths[position];
    if (path.empty()) {
      return failure(MSWindowsFileTransferSourceError::EmptyPath, position);
    }
    if (!path.is_absolute()) {
      return failure(MSWindowsFileTransferSourceError::RelativePath, position);
    }

    FileHandle handle(CreateFileW(
        path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr
    ));
    if (handle.get() == INVALID_HANDLE_VALUE) {
      return failure(MSWindowsFileTransferSourceError::OpenFailed, position);
    }

    BY_HANDLE_FILE_INFORMATION information{};
    if (GetFileInformationByHandle(handle.get(), &information) == 0) {
      return failure(MSWindowsFileTransferSourceError::MetadataReadFailed, position);
    }
    if ((information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
      return failure(MSWindowsFileTransferSourceError::ReparsePoint, position);
    }
    if ((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 || GetFileType(handle.get()) != FILE_TYPE_DISK) {
      return failure(MSWindowsFileTransferSourceError::NotRegularFile, position);
    }

    const auto name = utf8Filename(path);
    if (!name.has_value()) {
      return failure(MSWindowsFileTransferSourceError::FilenameConversionFailed, position);
    }

    FileTransferSourceCandidate source;
    source.path = path;
    source.identity = fileIdentity(information);
    source.size = combinedValue(information.nFileSizeHigh, information.nFileSizeLow);
    source.lastWriteTime =
        combinedValue(information.ftLastWriteTime.dwHighDateTime, information.ftLastWriteTime.dwLowDateTime);
    source.name = *name;
    sources.push_back(std::move(source));
  }

  return {
      .error = MSWindowsFileTransferSourceError::None,
      .itemPosition = std::nullopt,
      .sources = std::move(sources),
  };
}

} // namespace deskflow::filetransfer
