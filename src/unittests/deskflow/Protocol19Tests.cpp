/*
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Protocol19Tests.h"

#include "deskflow/CanonicalScancode.h"
#include "deskflow/InputLanguageTypes.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <string>
#include <vector>

namespace {

class MemoryStream : public deskflow::IStream
{
public:
  void close() override
  {
    m_closed = true;
  }

  uint32_t read(void *buffer, uint32_t size) override
  {
    if (m_closed || m_offset >= m_bytes.size()) {
      return 0;
    }
    const auto count = std::min<size_t>(size, m_bytes.size() - m_offset);
    std::memcpy(buffer, m_bytes.data() + m_offset, count);
    m_offset += count;
    return static_cast<uint32_t>(count);
  }

  void write(const void *buffer, uint32_t size) override
  {
    m_bytes.append(static_cast<const char *>(buffer), size);
  }

  void flush() override
  {
  }

  void shutdownInput() override
  {
    m_closed = true;
  }

  void shutdownOutput() override
  {
  }

  void *getEventTarget() const override
  {
    return const_cast<MemoryStream *>(this);
  }

  bool isReady() const override
  {
    return !m_closed && m_offset < m_bytes.size();
  }

  uint32_t getSize() const override
  {
    return static_cast<uint32_t>(m_bytes.size() - m_offset);
  }

private:
  std::string m_bytes;
  size_t m_offset = 0;
  bool m_closed = false;
};

} // namespace

void Protocol19Tests::inputLanguageControlRoundTrip()
{
  MemoryStream stream;
  const std::string target = "ko";
  ProtocolUtil::writef(
      &stream, kMsgDInputLangControl, static_cast<int8_t>(deskflow::InputLanguageAction::Set), &target
  );

  int8_t action = -1;
  std::string decodedTarget;
  QVERIFY(ProtocolUtil::readf(&stream, kMsgDInputLangControl, &action, &decodedTarget));
  QCOMPARE(action, static_cast<int8_t>(deskflow::InputLanguageAction::Set));
  QCOMPARE(decodedTarget, target);
}

void Protocol19Tests::inputLanguageStatusRoundTrip()
{
  MemoryStream stream;
  const std::string source = "com.apple.inputmethod.Korean.2SetKorean";
  ProtocolUtil::writef(
      &stream, kMsgCInputLangStatus, &source, static_cast<int8_t>(deskflow::InputLanguageCategory::InputMethod),
      static_cast<int8_t>(1)
  );

  std::string decodedSource;
  int8_t category = -1;
  int8_t composing = 0;
  QVERIFY(ProtocolUtil::readf(&stream, kMsgCInputLangStatus, &decodedSource, &category, &composing));
  QCOMPARE(decodedSource, source);
  QCOMPARE(category, static_cast<int8_t>(deskflow::InputLanguageCategory::InputMethod));
  QCOMPARE(composing, static_cast<int8_t>(1));
}

void Protocol19Tests::canonicalScancodeFlagRoundTrip()
{
  QCOMPARE(kProtocolCanonicalScancodeMinorVersion, static_cast<int16_t>(10));
  QVERIFY(kProtocolMinorVersion >= kProtocolCanonicalScancodeMinorVersion);

  MemoryStream stream;
  const uint16_t expectedId = 0x0061;
  const uint16_t expectedMask = 0;
  const auto expectedButton = deskflow::scancode::markCanonical(0x11d);
  ProtocolUtil::writef(
      &stream, kMsgDKeyDown, static_cast<uint32_t>(expectedId), static_cast<uint32_t>(expectedMask),
      static_cast<uint32_t>(expectedButton)
  );

  uint16_t id = 0;
  uint16_t mask = 0;
  uint16_t button = 0;
  QVERIFY(ProtocolUtil::readf(&stream, kMsgDKeyDown, &id, &mask, &button));
  QCOMPARE(id, expectedId);
  QCOMPARE(mask, expectedMask);
  QCOMPARE(button, expectedButton);
  QVERIFY(deskflow::scancode::isCanonical(button));
  QCOMPARE(deskflow::scancode::stripCanonical(button), KeyButton{0x11d});
}

void Protocol19Tests::displayLayoutRoundTrip()
{
  QCOMPARE(kProtocolDisplayLayoutMinorVersion, static_cast<int16_t>(11));
  QVERIFY(kProtocolMinorVersion >= kProtocolDisplayLayoutMinorVersion);

  MemoryStream stream;
  const std::vector<uint32_t> expected{std::bit_cast<uint32_t>(int32_t{-1920}), std::bit_cast<uint32_t>(int32_t{0}),
                                       std::bit_cast<uint32_t>(int32_t{1920}),  std::bit_cast<uint32_t>(int32_t{1080}),
                                       std::bit_cast<uint32_t>(int32_t{0}),     std::bit_cast<uint32_t>(int32_t{0}),
                                       std::bit_cast<uint32_t>(int32_t{2560}),  std::bit_cast<uint32_t>(int32_t{1440})};
  ProtocolUtil::writef(&stream, kMsgCDisplayLayout, &expected);

  std::vector<uint32_t> actual;
  QVERIFY(ProtocolUtil::readf(&stream, kMsgCDisplayLayout, &actual));
  QCOMPARE(actual, expected);
}

void Protocol19Tests::foregroundFullscreenRoundTrip()
{
  MemoryStream stream;
  ProtocolUtil::writef(&stream, kMsgCForegroundFullscreen, static_cast<int8_t>(1));

  int8_t actual = 0;
  QVERIFY(ProtocolUtil::readf(&stream, kMsgCForegroundFullscreen, &actual));
  QCOMPARE(actual, static_cast<int8_t>(1));
}

QTEST_MAIN(Protocol19Tests)
