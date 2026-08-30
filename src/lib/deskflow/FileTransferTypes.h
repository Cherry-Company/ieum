/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deskflow::filetransfer {

using TransferId = std::array<std::uint8_t, 16>;

struct FileTransferItem
{
  std::uint32_t index = 0;
  std::string name;
  std::uint64_t size = 0;
};

struct FileTransferOffer
{
  TransferId id{};
  std::string sourceScreen;
  std::string targetScreen;
  std::vector<FileTransferItem> items;
};

struct FileTransferLimits
{
  std::size_t maxItems = 100;
  std::size_t maxNameBytes = 1024;
  std::size_t maxScreenNameBytes = 255;
  std::uint64_t maxFileBytes = 20ULL * 1024 * 1024 * 1024;
  std::uint64_t maxTotalBytes = 50ULL * 1024 * 1024 * 1024;
};

enum class OfferError
{
  None,
  EmptyTransferId,
  EmptySourceScreen,
  EmptyTargetScreen,
  SameSourceAndTarget,
  ScreenNameTooLong,
  InvalidScreenNameUtf8,
  UnsafeScreenName,
  EmptyItems,
  TooManyItems,
  NonSequentialItemIndex,
  EmptyItemName,
  ItemNameTooLong,
  InvalidItemNameUtf8,
  UnsafeItemName,
  FileTooLarge,
  TotalSizeOverflow,
  TotalSizeTooLarge,
};

struct OfferValidation
{
  OfferError error = OfferError::None;
  std::optional<std::size_t> itemPosition;
  std::uint64_t totalBytes = 0;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == OfferError::None;
  }
};

} // namespace deskflow::filetransfer
