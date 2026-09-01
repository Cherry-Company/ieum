/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace deskflow::filetransfer {

struct FileTransferEdgeDrop
{
  Direction direction = Direction::NoDirection;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::vector<std::filesystem::path> paths;
};

using FileTransferEdgeDropHandler = std::function<void(FileTransferEdgeDrop)>;

} // namespace deskflow::filetransfer
