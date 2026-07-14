/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXClipboardAnyTextConverter.h"

#include "common/Settings.h"

#include <algorithm>

namespace {

std::string normalizeNfc(const std::string &value)
{
  const auto source = CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(value.data()), static_cast<CFIndex>(value.size()),
      kCFStringEncodingUTF8, false
  );
  if (source == nullptr) {
    return value;
  }
  const auto normalized = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, source);
  CFRelease(source);
  if (normalized == nullptr) {
    return value;
  }
  CFStringNormalize(normalized, kCFStringNormalizationFormC);

  const CFRange range = CFRangeMake(0, CFStringGetLength(normalized));
  CFIndex size = 0;
  CFStringGetBytes(normalized, range, kCFStringEncodingUTF8, 0, false, nullptr, 0, &size);
  std::string result(static_cast<size_t>(size), '\0');
  CFStringGetBytes(
      normalized, range, kCFStringEncodingUTF8, 0, false, reinterpret_cast<UInt8 *>(result.data()), size, nullptr
  );
  CFRelease(normalized);
  return result;
}

} // namespace

//
// OSXClipboardAnyTextConverter
//

IClipboard::Format OSXClipboardAnyTextConverter::getFormat() const
{
  return IClipboard::Format::Text;
}

std::string OSXClipboardAnyTextConverter::fromIClipboard(const std::string &data) const
{
  // convert linefeeds and then convert to desired encoding
  return doFromIClipboard(convertLinefeedToMacOS(data));
}

std::string OSXClipboardAnyTextConverter::toIClipboard(const std::string &data) const
{
  // convert text then newlines
  const auto converted = convertLinefeedToUnix(doToIClipboard(data));
  return Settings::value(Settings::Client::ClipboardNormalizeNfc).toBool() ? normalizeNfc(converted) : converted;
}

static bool isLF(char ch)
{
  return (ch == '\n');
}

static bool isCR(char ch)
{
  return (ch == '\r');
}

std::string OSXClipboardAnyTextConverter::convertLinefeedToMacOS(const std::string &src)
{
  // note -- we assume src is a valid UTF-8 string
  std::string copy = src;

  std::replace_if(copy.begin(), copy.end(), isLF, '\r');

  return copy;
}

std::string OSXClipboardAnyTextConverter::convertLinefeedToUnix(const std::string &src)
{
  std::string copy = src;

  std::replace_if(copy.begin(), copy.end(), isCR, '\n');

  return copy;
}
