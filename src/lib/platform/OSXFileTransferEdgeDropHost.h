/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/DisplayGeometry.h"
#include "deskflow/FileTransferEdgeDrop.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#ifdef __OBJC__
@class NSArray;
@class NSURL;
#endif

namespace deskflow::filetransfer {

struct OSXFileTransferEdgeDropWindow
{
  Direction direction = Direction::NoDirection;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;

  bool operator==(const OSXFileTransferEdgeDropWindow &) const = default;
};

[[nodiscard]] std::vector<OSXFileTransferEdgeDropWindow> makeOSXFileTransferEdgeDropLayout(
    const deskflow::DisplayLayout &displays, std::uint32_t activeSides, std::int32_t thickness = 3
);

struct OSXFileTransferDropPoint
{
  std::int32_t x = 0;
  std::int32_t y = 0;
  bool operator==(const OSXFileTransferDropPoint &) const = default;
};

[[nodiscard]] OSXFileTransferDropPoint appKitToIeumFileDropPoint(double x, double y, double appKitDesktopTop) noexcept;

enum class OSXFileDropError
{
  None,
  Empty,
  TooManyItems,
  RemoteUrl,
  DirectoryOrPackage,
  InvalidPath,
};

struct OSXFileDropExtraction
{
  OSXFileDropError error = OSXFileDropError::None;
  std::vector<std::filesystem::path> paths;
  [[nodiscard]] bool ok() const noexcept
  {
    return error == OSXFileDropError::None;
  }
};

#ifdef __OBJC__
[[nodiscard]] OSXFileDropExtraction extractOSXFileDropURLs(NSArray *urls, std::size_t maxItems = 100);
#endif

class OSXFileTransferEdgeDropHost final
{
public:
  explicit OSXFileTransferEdgeDropHost(
      FileTransferEdgeDropHandler handler, std::chrono::milliseconds dwell = std::chrono::milliseconds{250}
  );
  ~OSXFileTransferEdgeDropHost();

  OSXFileTransferEdgeDropHost(const OSXFileTransferEdgeDropHost &) = delete;
  OSXFileTransferEdgeDropHost &operator=(const OSXFileTransferEdgeDropHost &) = delete;

  [[nodiscard]] bool configure(const deskflow::DisplayLayout &displays, std::uint32_t activeSides);
  void clear() noexcept;
  [[nodiscard]] std::size_t windowCount() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::filetransfer
