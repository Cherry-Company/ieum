/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UnicodeTests.h"

#include "base/Unicode.h"

#include <QByteArray>

void UnicodeTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Verbose);
}

void UnicodeTests::UTF16ToUTF8()
{
  bool errors;
  auto result = Unicode::UTF16ToUTF8(std::string("h\0e\0l\0l\0o\0", 10), &errors);

  QVERIFY(!errors);
  QCOMPARE(result.c_str(), "hello");
}

void UnicodeTests::acceptsRFC3629Boundaries()
{
  const QList<QByteArray> validSequences = {
      QByteArray{},
      QByteArray::fromHex("00"),
      QByteArray::fromHex("7f"),
      QByteArray::fromHex("c280"),
      QByteArray::fromHex("dfbf"),
      QByteArray::fromHex("e0a080"),
      QByteArray::fromHex("ed9fbf"),
      QByteArray::fromHex("ee8080"),
      QByteArray::fromHex("efbfbd"),
      QByteArray::fromHex("f0908080"),
      QByteArray::fromHex("f48fbfbf"),
  };

  for (const auto &bytes : validSequences) {
    const std::string input(bytes.constData(), static_cast<size_t>(bytes.size()));
    QVERIFY2(Unicode::isUTF8(input), bytes.toHex().constData());
  }
}

void UnicodeTests::rejectsInvalidUTF8_data()
{
  QTest::addColumn<QByteArray>("bytes");

  QTest::newRow("standalone-continuation") << QByteArray::fromHex("80");
  QTest::newRow("overlong-two-byte") << QByteArray::fromHex("c080");
  QTest::newRow("overlong-two-byte-c1") << QByteArray::fromHex("c1bf");
  QTest::newRow("overlong-three-byte") << QByteArray::fromHex("e08080");
  QTest::newRow("overlong-four-byte") << QByteArray::fromHex("f0808080");
  QTest::newRow("surrogate") << QByteArray::fromHex("eda080");
  QTest::newRow("above-unicode-maximum") << QByteArray::fromHex("f4908080");
  QTest::newRow("f5-leader") << QByteArray::fromHex("f5808080");
  QTest::newRow("five-byte-sequence") << QByteArray::fromHex("f888808080");
  QTest::newRow("six-byte-sequence") << QByteArray::fromHex("fc8480808080");
  QTest::newRow("fe-leader") << QByteArray::fromHex("fe");
  QTest::newRow("ff-leader") << QByteArray::fromHex("ff");
  QTest::newRow("bad-first-continuation") << QByteArray::fromHex("f0418080");
  QTest::newRow("bad-second-continuation") << QByteArray::fromHex("f0904180");
  QTest::newRow("bad-third-continuation") << QByteArray::fromHex("f0908041");
  QTest::newRow("truncated-four-byte") << QByteArray::fromHex("f09080");
}

void UnicodeTests::rejectsInvalidUTF8()
{
  QFETCH(QByteArray, bytes);
  const std::string input(bytes.constData(), static_cast<size_t>(bytes.size()));

  QVERIFY(!Unicode::isUTF8(input));
}

void UnicodeTests::maximumScalarRoundTrips()
{
  const auto maximumScalar = QByteArray::fromHex("f48fbfbf");
  const std::string utf8(maximumScalar.constData(), static_cast<size_t>(maximumScalar.size()));

  bool decodingErrors = true;
  const auto utf16 = Unicode::UTF8ToUTF16(utf8, &decodingErrors);
  QVERIFY(!decodingErrors);

  bool encodingErrors = true;
  const auto roundTrip = Unicode::UTF16ToUTF8(utf16, &encodingErrors);
  QVERIFY(!encodingErrors);
  QCOMPARE(QByteArray::fromStdString(roundTrip), maximumScalar);
}

void UnicodeTests::encoderRejectsNonScalarValues_data()
{
  QTest::addColumn<quint32>("codePoint");

  QTest::newRow("high-surrogate") << quint32{0xd800};
  QTest::newRow("one-above-maximum") << quint32{0x110000};
  QTest::newRow("former-five-byte-minimum") << quint32{0x200000};
  QTest::newRow("former-six-byte-minimum") << quint32{0x4000000};
  QTest::newRow("largest-former-six-byte-value") << quint32{0x7fffffff};
}

void UnicodeTests::encoderRejectsNonScalarValues()
{
  QFETCH(quint32, codePoint);
  std::string encoded;
  bool errors = false;

  Unicode::toUTF8(encoded, codePoint, &errors);

  QVERIFY(errors);
  QCOMPARE(QByteArray::fromStdString(encoded), QByteArray::fromHex("efbfbd"));
}

QTEST_MAIN(UnicodeTests)
