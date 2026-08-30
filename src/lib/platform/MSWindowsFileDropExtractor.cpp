/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsFileDropExtractor.h"

#include <ole2.h>

#include <limits>
#include <string>
#include <utility>

namespace deskflow::filetransfer {
namespace {

WindowsFileDropExtraction failure(WindowsFileDropError error, std::optional<std::size_t> itemPosition = std::nullopt)
{
  return {.error = error, .itemPosition = itemPosition, .paths = {}};
}

class StgMediumGuard
{
public:
  explicit StgMediumGuard(STGMEDIUM &medium) : m_medium(medium)
  {
  }

  StgMediumGuard(const StgMediumGuard &) = delete;
  StgMediumGuard &operator=(const StgMediumGuard &) = delete;

  ~StgMediumGuard()
  {
    ReleaseStgMedium(&m_medium);
  }

private:
  STGMEDIUM &m_medium;
};

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

WindowsFileDropExtraction extractWindowsFileDropDataObject(IDataObject *dataObject, const WindowsFileDropLimits &limits)
{
  if (dataObject == nullptr) {
    return failure(WindowsFileDropError::NullDataObject);
  }

  FORMATETC format{
      .cfFormat = CF_HDROP,
      .ptd = nullptr,
      .dwAspect = DVASPECT_CONTENT,
      .lindex = -1,
      .tymed = TYMED_HGLOBAL,
  };
  if (dataObject->QueryGetData(&format) != S_OK) {
    return failure(WindowsFileDropError::UnsupportedFormat);
  }

  STGMEDIUM medium{};
  if (dataObject->GetData(&format, &medium) != S_OK) {
    return failure(WindowsFileDropError::DataRequestFailed);
  }
  const StgMediumGuard mediumGuard(medium);

  if (medium.tymed != TYMED_HGLOBAL || medium.hGlobal == nullptr) {
    return failure(WindowsFileDropError::InvalidStorage);
  }

  return extractWindowsFileDrop(reinterpret_cast<HDROP>(medium.hGlobal), limits);
}

} // namespace deskflow::filetransfer
