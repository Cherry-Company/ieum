/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferOfferValidator.h"

#include <QByteArray>
#include <QTest>

#include <cstdint>
#include <limits>
#include <string>

using namespace deskflow::filetransfer;

namespace {

FileTransferOffer validOffer()
{
  FileTransferOffer offer;
  offer.id[0] = 0x42;
  offer.sourceScreen = "source";
  offer.targetScreen = "target";
  offer.items = {
      {0, "한글.txt", 0},
      {1, "photo 01.png", 17},
  };
  return offer;
}

std::string bytes(const QByteArray &value)
{
  return {value.constData(), static_cast<size_t>(value.size())};
}

} // namespace

class FileTransferOfferTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void acceptsUnicodeAndZeroByteFiles()
  {
    auto offer = validOffer();
    offer.sourceScreen = "작업실";
    offer.targetScreen = "거실 PC";

    const auto result = validateOffer(offer);

    QVERIFY(result.ok());
    QCOMPARE(result.error, OfferError::None);
    QCOMPARE(result.totalBytes, std::uint64_t{17});
    QVERIFY(!result.itemPosition.has_value());
  }

  void rejectsMissingIdentityAndInvalidScreens_data()
  {
    QTest::addColumn<int>("mutation");
    QTest::addColumn<int>("expectedError");

    QTest::newRow("empty-transfer-id") << 0 << static_cast<int>(OfferError::EmptyTransferId);
    QTest::newRow("empty-source") << 1 << static_cast<int>(OfferError::EmptySourceScreen);
    QTest::newRow("empty-target") << 2 << static_cast<int>(OfferError::EmptyTargetScreen);
    QTest::newRow("same-source-target") << 3 << static_cast<int>(OfferError::SameSourceAndTarget);
    QTest::newRow("source-too-long") << 4 << static_cast<int>(OfferError::ScreenNameTooLong);
    QTest::newRow("target-too-long") << 5 << static_cast<int>(OfferError::ScreenNameTooLong);
    QTest::newRow("source-invalid-utf8") << 6 << static_cast<int>(OfferError::InvalidScreenNameUtf8);
    QTest::newRow("target-invalid-utf8") << 7 << static_cast<int>(OfferError::InvalidScreenNameUtf8);
    QTest::newRow("source-embedded-nul") << 8 << static_cast<int>(OfferError::UnsafeScreenName);
    QTest::newRow("target-control-byte") << 9 << static_cast<int>(OfferError::UnsafeScreenName);
  }

  void rejectsMissingIdentityAndInvalidScreens()
  {
    QFETCH(int, mutation);
    QFETCH(int, expectedError);

    auto offer = validOffer();
    FileTransferLimits limits;
    limits.maxScreenNameBytes = 6;

    switch (mutation) {
    case 0:
      offer.id = {};
      break;
    case 1:
      offer.sourceScreen.clear();
      break;
    case 2:
      offer.targetScreen.clear();
      break;
    case 3:
      offer.targetScreen = offer.sourceScreen;
      break;
    case 4:
      offer.sourceScreen = "1234567";
      break;
    case 5:
      offer.targetScreen = "1234567";
      break;
    case 6:
      offer.sourceScreen = bytes(QByteArray::fromHex("f5808080"));
      break;
    case 7:
      offer.targetScreen = bytes(QByteArray::fromHex("eda080"));
      break;
    case 8:
      offer.sourceScreen = std::string("src\0pc", 6);
      break;
    case 9:
      offer.targetScreen = std::string("bad\x1fpc", 6);
      break;
    default:
      QFAIL("unknown mutation");
    }

    const auto result = validateOffer(offer, limits);

    QCOMPARE(static_cast<int>(result.error), expectedError);
    QVERIFY(!result.ok());
    QVERIFY(!result.itemPosition.has_value());
  }

  void rejectsEmptyAndOversizedItemLists()
  {
    auto offer = validOffer();
    offer.items.clear();
    QCOMPARE(validateOffer(offer).error, OfferError::EmptyItems);

    offer = validOffer();
    FileTransferLimits limits;
    limits.maxItems = 1;
    QCOMPARE(validateOffer(offer, limits).error, OfferError::TooManyItems);
  }

  void rejectsNonSequentialIndices_data()
  {
    QTest::addColumn<quint32>("firstIndex");
    QTest::addColumn<quint32>("secondIndex");
    QTest::addColumn<qulonglong>("expectedPosition");

    QTest::newRow("first-index-is-not-zero") << quint32{1} << quint32{1} << qulonglong{0};
    QTest::newRow("duplicate-index") << quint32{0} << quint32{0} << qulonglong{1};
    QTest::newRow("index-gap") << quint32{0} << quint32{2} << qulonglong{1};
  }

  void rejectsNonSequentialIndices()
  {
    QFETCH(quint32, firstIndex);
    QFETCH(quint32, secondIndex);
    QFETCH(qulonglong, expectedPosition);

    auto offer = validOffer();
    offer.items[0].index = firstIndex;
    offer.items[1].index = secondIndex;

    const auto result = validateOffer(offer);

    QCOMPARE(result.error, OfferError::NonSequentialItemIndex);
    QVERIFY(result.itemPosition.has_value());
    QCOMPARE(static_cast<qulonglong>(*result.itemPosition), expectedPosition);
  }

  void rejectsUnsafeNames_data()
  {
    QTest::addColumn<QByteArray>("name");
    QTest::addColumn<int>("expectedError");

    QTest::newRow("empty") << QByteArray{} << static_cast<int>(OfferError::EmptyItemName);
    QTest::newRow("dot") << QByteArray(".") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("dot-dot") << QByteArray("..") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("parent-component") << QByteArray("../a") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("forward-slash") << QByteArray("a/b") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("backslash") << QByteArray("a\\b") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("embedded-nul") << QByteArray("bad\0name.txt", 12) << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("control-byte") << QByteArray(
                                         "bad\x1f"
                                         "name.txt",
                                         12
                                     )
                                  << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("invalid-angle") << QByteArray("bad<name.txt") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("invalid-colon") << QByteArray("bad:name.txt") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("invalid-quote") << QByteArray("bad\"name.txt") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("invalid-pipe") << QByteArray("bad|name.txt") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("invalid-question") << QByteArray("bad?name.txt") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("invalid-star") << QByteArray("bad*name.txt") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("delete-byte") << QByteArray(
                                        "bad\x7f"
                                        "name.txt",
                                        12
                                    )
                                 << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("trailing-dot") << QByteArray("trailing.") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("trailing-space") << QByteArray("trailing ") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("reserved-con") << QByteArray("CON") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("reserved-con-extension") << QByteArray("con.txt") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("reserved-nul") << QByteArray("NUL.bin") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("reserved-com1") << QByteArray("COM1") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("reserved-lpt9") << QByteArray("LPT9.log") << static_cast<int>(OfferError::UnsafeItemName);
    QTest::newRow("valid-console") << QByteArray("console.txt") << static_cast<int>(OfferError::None);
    QTest::newRow("valid-com10") << QByteArray("COM10.txt") << static_cast<int>(OfferError::None);
    QTest::newRow("valid-korean") << QByteArray("한글 문서.txt") << static_cast<int>(OfferError::None);
  }

  void rejectsUnsafeNames()
  {
    QFETCH(QByteArray, name);
    QFETCH(int, expectedError);

    auto offer = validOffer();
    offer.items = {{0, bytes(name), 1}};

    const auto result = validateOffer(offer);

    QCOMPARE(static_cast<int>(result.error), expectedError);
    if (expectedError == static_cast<int>(OfferError::None)) {
      QVERIFY(result.ok());
      QVERIFY(!result.itemPosition.has_value());
    } else {
      QVERIFY(!result.ok());
      QCOMPARE(result.itemPosition, std::optional<std::size_t>{0});
    }
  }

  void rejectsInvalidUtf8AndNameLength()
  {
    auto offer = validOffer();
    offer.items = {{0, bytes(QByteArray::fromHex("f5808080")), 1}};
    QCOMPARE(validateOffer(offer).error, OfferError::InvalidItemNameUtf8);

    FileTransferLimits limits;
    limits.maxNameBytes = 3;
    offer.items = {{0, "four", 1}};
    QCOMPARE(validateOffer(offer, limits).error, OfferError::ItemNameTooLong);

    limits.maxNameBytes = 4;
    const auto exactBoundary = validateOffer(offer, limits);
    QVERIFY(exactBoundary.ok());
    QCOMPARE(exactBoundary.totalBytes, std::uint64_t{1});
  }

  void rejectsPerFileAndAggregateLimits()
  {
    FileTransferLimits limits;
    limits.maxFileBytes = 10;
    limits.maxTotalBytes = 15;

    auto offer = validOffer();
    offer.items = {{0, "large.bin", 11}};
    auto result = validateOffer(offer, limits);
    QCOMPARE(result.error, OfferError::FileTooLarge);
    QCOMPARE(result.itemPosition, std::optional<std::size_t>{0});

    offer.items = {{0, "first.bin", 8}, {1, "second.bin", 8}};
    result = validateOffer(offer, limits);
    QCOMPARE(result.error, OfferError::TotalSizeTooLarge);
    QCOMPARE(result.itemPosition, std::optional<std::size_t>{1});
    QCOMPARE(result.totalBytes, std::uint64_t{16});

    offer.items = {{0, "first.bin", 5}, {1, "second.bin", 10}};
    result = validateOffer(offer, limits);
    QVERIFY(result.ok());
    QCOMPARE(result.totalBytes, std::uint64_t{15});
  }

  void detectsTotalSizeOverflow()
  {
    FileTransferLimits limits;
    limits.maxFileBytes = std::numeric_limits<std::uint64_t>::max();
    limits.maxTotalBytes = std::numeric_limits<std::uint64_t>::max();

    auto offer = validOffer();
    offer.items = {
        {0, "maximum.bin", std::numeric_limits<std::uint64_t>::max()},
        {1, "one-more.bin", 1},
    };

    const auto result = validateOffer(offer, limits);

    QCOMPARE(result.error, OfferError::TotalSizeOverflow);
    QCOMPARE(result.itemPosition, std::optional<std::size_t>{1});
    QCOMPARE(result.totalBytes, std::numeric_limits<std::uint64_t>::max());
  }
};

QTEST_GUILESS_MAIN(FileTransferOfferTests)

#include "FileTransferOfferTests.moc"
