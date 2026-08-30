/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsFileDropExtractor.h"

#include <limits>
#include <string>
#include <utility>

namespace deskflow::filetransfer {
namespace {

WindowsFileDropExtraction failure(WindowsFileDropError error, std::optional<std::size_t> itemPosition = std::nullopt)
{
  return {.error = error, .itemPosition = itemPosition, .paths = {}};
}

} // namespace

WindowsFileDropExtraction extractWindowsFileDrop(HDROP drop, const WindowsFileDropLimits &limits)
{
  if (drop == nullptr) {
    return failure(WindowsFileDropError::NullHandle);
  }

  constexpr auto queryItemCount = (std::numeric_limits<UINT>::max)();
  const auto itemCount = DragQueryFileW(drop, queryItemCount, nullptr, 0);
  if (itemCount == 0) {
    return failure(WindowsFileDropError::EmptyDrop);
  }
  if (itemCount > limits.maxItems) {
    return failure(WindowsFileDropError::TooManyItems);
  }

  std::vector<std::filesystem::path> paths;
  paths.reserve(itemCount);

  for (UINT itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
    const auto pathLength = DragQueryFileW(drop, itemIndex, nullptr, 0);
    const auto itemPosition = static_cast<std::size_t>(itemIndex);
    if (pathLength == 0 || pathLength == (std::numeric_limits<UINT>::max)()) {
      return failure(WindowsFileDropError::PathQueryFailed, itemPosition);
    }
    if (pathLength > limits.maxPathCharacters) {
      return failure(WindowsFileDropError::PathTooLong, itemPosition);
    }

    std::wstring path(static_cast<std::size_t>(pathLength) + 1, L'\0');
    const auto capacity = static_cast<UINT>(path.size());
    const auto copiedLength = DragQueryFileW(drop, itemIndex, path.data(), capacity);
    if (copiedLength != pathLength) {
      return failure(WindowsFileDropError::PathQueryFailed, itemPosition);
    }

    path.resize(copiedLength);
    paths.emplace_back(std::move(path));
  }

  return {.error = WindowsFileDropError::None, .itemPosition = std::nullopt, .paths = std::move(paths)};
}

} // namespace deskflow::filetransfer
