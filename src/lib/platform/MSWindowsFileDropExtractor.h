/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shellapi.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace deskflow::filetransfer {

struct WindowsFileDropLimits
{
  std::size_t maxItems = 100;
  std::size_t maxPathCharacters = 32767;
};

enum class WindowsFileDropError
{
  None,
  NullHandle,
  EmptyDrop,
  TooManyItems,
  PathTooLong,
  PathQueryFailed,
};

struct WindowsFileDropExtraction
{
  WindowsFileDropError error = WindowsFileDropError::None;
  std::optional<std::size_t> itemPosition;
  std::vector<std::filesystem::path> paths;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == WindowsFileDropError::None;
  }
};

[[nodiscard]] WindowsFileDropExtraction extractWindowsFileDrop(HDROP drop, const WindowsFileDropLimits &limits = {});

} // namespace deskflow::filetransfer
