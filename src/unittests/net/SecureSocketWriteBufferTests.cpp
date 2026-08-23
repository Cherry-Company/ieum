/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/SecureSocketWriteBuffer.h"

#include <QTest>

#include <cstdlib>
#include <cstring>

namespace {
int g_reallocCalls = 0;

void *failSecondReallocation(void *current, size_t requested)
{
  ++g_reallocCalls;
  if (g_reallocCalls == 2) {
    return nullptr;
  }
  return std::realloc(current, requested);
}
} // namespace

class SecureSocketWriteBufferTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void keepsRetryPayloadConnectionLocal()
  {
    const char firstBytes[] = "first";
    const char secondBytes[] = "second";
    SecureSocketWriteBuffer first;
    SecureSocketWriteBuffer second;

    QVERIFY(first.capture(firstBytes, sizeof(firstBytes)));
    first.markRetry();
    QVERIFY(second.capture(secondBytes, sizeof(secondBytes)));

    QVERIFY(first.retrying());
    QVERIFY(!second.retrying());
    QCOMPARE(first.size(), uint32_t{sizeof(firstBytes)});
    QCOMPARE(second.size(), uint32_t{sizeof(secondBytes)});
    QCOMPARE(std::memcmp(first.data(), firstBytes, sizeof(firstBytes)), 0);
    QCOMPARE(std::memcmp(second.data(), secondBytes, sizeof(secondBytes)), 0);
  }

  void preservesPayloadWhenGrowthFails()
  {
    g_reallocCalls = 0;
    const char original[] = "old";
    const char larger[] = "larger payload";
    SecureSocketWriteBuffer buffer{failSecondReallocation};

    QVERIFY(buffer.capture(original, sizeof(original)));
    QVERIFY(!buffer.capture(larger, sizeof(larger)));

    QCOMPARE(buffer.size(), uint32_t{sizeof(original)});
    QCOMPARE(std::memcmp(buffer.data(), original, sizeof(original)), 0);
    QCOMPARE(g_reallocCalls, 2);
  }

  void clearsOnlyTheCompletedConnection()
  {
    const char bytes[] = "payload";
    SecureSocketWriteBuffer first;
    SecureSocketWriteBuffer second;
    QVERIFY(first.capture(bytes, sizeof(bytes)));
    QVERIFY(second.capture(bytes, sizeof(bytes)));
    first.markRetry();
    second.markRetry();

    first.clearRetry();

    QVERIFY(!first.retrying());
    QVERIFY(second.retrying());
  }
};

QTEST_GUILESS_MAIN(SecureSocketWriteBufferTests)

#include "SecureSocketWriteBufferTests.moc"
