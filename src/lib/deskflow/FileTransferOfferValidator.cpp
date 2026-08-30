/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferOfferValidator.h"

#include "base/Unicode.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string_view>

namespace deskflow::filetransfer {
namespace {

OfferValidation failure(OfferError error, std::uint64_t totalBytes = 0)
{
  return {.error = error, .itemPosition = std::nullopt, .totalBytes = totalBytes};
}

OfferValidation itemFailure(OfferError error, std::size_t position, std::uint64_t totalBytes)
{
  return {.error = error, .itemPosition = position, .totalBytes = totalBytes};
}

bool isEmptyTransferId(const TransferId &id)
{
  return std::ranges::all_of(id, [](const auto byte) { return byte == 0; });
}

bool hasUnsafeIdentityByte(const std::string_view value)
{
  return std::ranges::any_of(value, [](const char valueByte) {
    const auto byte = static_cast<unsigned char>(valueByte);
    return byte < 0x20 || byte == 0x7f;
  });
}

bool isInvalidWindowsNameByte(const unsigned char byte)
{
  if (byte < 0x20 || byte == 0x7f) {
    return true;
  }

  switch (byte) {
  case '<':
  case '>':
  case ':':
  case '"':
  case '/':
  case '\\':
  case '|':
  case '?':
  case '*':
    return true;

  default:
    return false;
  }
}

std::string asciiUpper(std::string value)
{
  std::ranges::transform(value, value.begin(), [](const char valueByte) {
    const auto byte = static_cast<unsigned char>(valueByte);
    return byte < 0x80 ? static_cast<char>(std::toupper(byte)) : valueByte;
  });
  return value;
}

bool isReservedWindowsDeviceName(const std::string_view name)
{
  auto basename = std::string(name.substr(0, name.find('.')));
  while (!basename.empty() && (basename.back() == ' ' || basename.back() == '.')) {
    basename.pop_back();
  }
  basename = asciiUpper(std::move(basename));

  if (basename == "CON" || basename == "PRN" || basename == "AUX" || basename == "NUL") {
    return true;
  }

  if (basename.size() != 4 || (basename[3] < '1' || basename[3] > '9')) {
    return false;
  }

  return basename.starts_with("COM") || basename.starts_with("LPT");
}

bool isSafeItemName(const std::string_view name)
{
  if (name == "." || name == ".." || name.ends_with('.') || name.ends_with(' ')) {
    return false;
  }

  if (std::ranges::any_of(name, [](const char valueByte) {
        return isInvalidWindowsNameByte(static_cast<unsigned char>(valueByte));
      })) {
    return false;
  }

  return !isReservedWindowsDeviceName(name);
}

} // namespace

OfferValidation validateTransferRoute(
    const TransferId &id, const std::string &sourceScreen, const std::string &targetScreen,
    const FileTransferLimits &limits
)
{
  if (isEmptyTransferId(id)) {
    return failure(OfferError::EmptyTransferId);
  }

  if (sourceScreen.empty()) {
    return failure(OfferError::EmptySourceScreen);
  }
  if (targetScreen.empty()) {
    return failure(OfferError::EmptyTargetScreen);
  }

  if (sourceScreen.size() > limits.maxScreenNameBytes || targetScreen.size() > limits.maxScreenNameBytes) {
    return failure(OfferError::ScreenNameTooLong);
  }

  if (!Unicode::isUTF8(sourceScreen) || !Unicode::isUTF8(targetScreen)) {
    return failure(OfferError::InvalidScreenNameUtf8);
  }

  if (hasUnsafeIdentityByte(sourceScreen) || hasUnsafeIdentityByte(targetScreen)) {
    return failure(OfferError::UnsafeScreenName);
  }

  if (sourceScreen == targetScreen) {
    return failure(OfferError::SameSourceAndTarget);
  }

  return {};
}

OfferValidation validateOffer(const FileTransferOffer &offer, const FileTransferLimits &limits)
{
  const auto routeValidation = validateTransferRoute(offer.id, offer.sourceScreen, offer.targetScreen, limits);
  if (!routeValidation.ok()) {
    return routeValidation;
  }

  if (offer.items.empty()) {
    return failure(OfferError::EmptyItems);
  }
  if (offer.items.size() > limits.maxItems) {
    return failure(OfferError::TooManyItems);
  }

  std::uint64_t totalBytes = 0;
  for (std::size_t position = 0; position < offer.items.size(); ++position) {
    const auto &item = offer.items[position];

    if (item.index != position) {
      return itemFailure(OfferError::NonSequentialItemIndex, position, totalBytes);
    }
    if (item.name.empty()) {
      return itemFailure(OfferError::EmptyItemName, position, totalBytes);
    }
    if (item.name.size() > limits.maxNameBytes) {
      return itemFailure(OfferError::ItemNameTooLong, position, totalBytes);
    }
    if (!Unicode::isUTF8(item.name)) {
      return itemFailure(OfferError::InvalidItemNameUtf8, position, totalBytes);
    }
    if (!isSafeItemName(item.name)) {
      return itemFailure(OfferError::UnsafeItemName, position, totalBytes);
    }
    if (item.size > limits.maxFileBytes) {
      return itemFailure(OfferError::FileTooLarge, position, totalBytes);
    }
    if (item.size > std::numeric_limits<std::uint64_t>::max() - totalBytes) {
      return itemFailure(OfferError::TotalSizeOverflow, position, totalBytes);
    }

    totalBytes += item.size;
    if (totalBytes > limits.maxTotalBytes) {
      return itemFailure(OfferError::TotalSizeTooLarge, position, totalBytes);
    }
  }

  return {.error = OfferError::None, .itemPosition = std::nullopt, .totalBytes = totalBytes};
}

} // namespace deskflow::filetransfer
