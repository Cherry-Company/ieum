/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/FileTransferEdgeDropIpc.h"
#include "common/IpcMessage.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTest>

namespace {

using deskflow::ipc::FileTransferEdgeDropIpcError;
using deskflow::ipc::FileTransferEdgeDropIpcValue;

QString absoluteNativePath(const QString &relativePath)
{
  return QDir::toNativeSeparators(QDir::current().absoluteFilePath(relativePath));
}

QString encodeFixtureJson(const QByteArray &json)
{
  return QString::fromLatin1(json.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QByteArray decodeFixtureBase64Url(const QString &encoded)
{
  auto padded = encoded.toLatin1();
  while ((padded.size() % 4) != 0) {
    padded.append('=');
  }
  return QByteArray::fromBase64(padded, QByteArray::Base64UrlEncoding);
}

QStringList fixturePaths()
{
  return {
      absoluteNativePath(QStringLiteral("tmp/edge drop/space path/alpha one.txt")),
      absoluteNativePath(QStringLiteral("tmp/edge drop/한글 경로/둘.txt")),
  };
}

FileTransferEdgeDropIpcValue fixtureValue(Direction direction)
{
  return {
      .direction = direction,
      .x = -120,
      .y = 2048,
      .paths = fixturePaths(),
  };
}

QString encodeJsonObject(const QJsonObject &object)
{
  return encodeFixtureJson(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QJsonObject validJsonObject(const QJsonArray &paths)
{
  return {
      {QStringLiteral("v"), 1},     {QStringLiteral("d"), static_cast<int>(Direction::Left)},
      {QStringLiteral("x"), 0},     {QStringLiteral("y"), 0},
      {QStringLiteral("p"), paths},
  };
}

QString encodeMutatedJsonObject(const QJsonArray &paths, const QString &key, const QJsonValue &value)
{
  auto object = validJsonObject(paths);
  object.insert(key, value);
  return encodeJsonObject(object);
}

} // namespace

class FileTransferEdgeDropIpcTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void roundTripsAbsoluteUnicodePaths_data();
  void roundTripsAbsoluteUnicodePaths();
  void acceptsMaximumPathCount();
  void encodesCompactUnpaddedBase64UrlJson();
  void rejectsInvalidEncodeInputs();
  void rejectsInvalidDecodeInputs_data();
  void rejectsInvalidDecodeInputs();
  void sanitizesSensitiveEdgeDropMessages();
};

void FileTransferEdgeDropIpcTests::roundTripsAbsoluteUnicodePaths_data()
{
  QTest::addColumn<int>("direction");

  QTest::newRow("left") << static_cast<int>(Direction::Left);
  QTest::newRow("right") << static_cast<int>(Direction::Right);
  QTest::newRow("top") << static_cast<int>(Direction::Top);
  QTest::newRow("bottom") << static_cast<int>(Direction::Bottom);
}

void FileTransferEdgeDropIpcTests::roundTripsAbsoluteUnicodePaths()
{
  QFETCH(int, direction);

  const auto value = fixtureValue(static_cast<Direction>(direction));
  const auto encoded = deskflow::ipc::encodeFileTransferEdgeDropIpc(value);
  QCOMPARE(encoded.error, FileTransferEdgeDropIpcError::None);
  QVERIFY(!encoded.encodedValue.isEmpty());
  QVERIFY(!encoded.encodedValue.contains(QLatin1Char('=')));
  QVERIFY(!encoded.encodedValue.contains(QLatin1Char('+')));
  QVERIFY(!encoded.encodedValue.contains(QLatin1Char('/')));

  const auto repeated = deskflow::ipc::encodeFileTransferEdgeDropIpc(value);
  QCOMPARE(repeated.error, FileTransferEdgeDropIpcError::None);
  QCOMPARE(repeated.encodedValue, encoded.encodedValue);

  const auto decoded = deskflow::ipc::decodeFileTransferEdgeDropIpc(encoded.encodedValue);
  QCOMPARE(decoded.error, FileTransferEdgeDropIpcError::None);
  QVERIFY(decoded.value.has_value());
  QCOMPARE(*decoded.value, value);
}

void FileTransferEdgeDropIpcTests::acceptsMaximumPathCount()
{
  FileTransferEdgeDropIpcValue value{
      .direction = Direction::Right,
      .x = 17,
      .y = -33,
  };

  for (int index = 0; index < 100; ++index) {
    value.paths.append(absoluteNativePath(QStringLiteral("tmp/edge drop/max/file-%1.txt").arg(index)));
  }

  const auto encoded = deskflow::ipc::encodeFileTransferEdgeDropIpc(value);
  QCOMPARE(encoded.error, FileTransferEdgeDropIpcError::None);
  QVERIFY(encoded.encodedValue.toUtf8().size() <= 60 * 1024);

  const auto decoded = deskflow::ipc::decodeFileTransferEdgeDropIpc(encoded.encodedValue);
  QCOMPARE(decoded.error, FileTransferEdgeDropIpcError::None);
  QVERIFY(decoded.value.has_value());
  QCOMPARE(*decoded.value, value);
}

void FileTransferEdgeDropIpcTests::encodesCompactUnpaddedBase64UrlJson()
{
  const FileTransferEdgeDropIpcValue value{
      .direction = Direction::Left,
      .x = -7,
      .y = 42,
      .paths = {absoluteNativePath(QStringLiteral("tmp/edge drop/한글 path/file name.txt"))},
  };

  const auto encoded = deskflow::ipc::encodeFileTransferEdgeDropIpc(value);
  QCOMPARE(encoded.error, FileTransferEdgeDropIpcError::None);

  QJsonParseError parseError;
  const auto json = decodeFixtureBase64Url(encoded.encodedValue);
  const auto document = QJsonDocument::fromJson(json, &parseError);
  QCOMPARE(parseError.error, QJsonParseError::NoError);
  QVERIFY(document.isObject());
  QCOMPARE(json.contains('\n'), false);
  QCOMPARE(json.contains('\r'), false);

  const auto expectedJson =
      QStringLiteral(R"({"v":1,"d":1,"x":-7,"y":42,"p":%1})")
          .arg(QString::fromUtf8(QJsonDocument(QJsonArray{value.paths.first()}).toJson(QJsonDocument::Compact)))
          .toUtf8();
  QCOMPARE(json, expectedJson);

  const auto object = document.object();
  auto keys = object.keys();
  keys.sort();
  QCOMPARE(
      keys,
      QStringList(
          {QStringLiteral("d"), QStringLiteral("p"), QStringLiteral("v"), QStringLiteral("x"), QStringLiteral("y")}
      )
  );
  QCOMPARE(object.value(QStringLiteral("v")).toInt(), 1);
  QCOMPARE(object.value(QStringLiteral("d")).toInt(), static_cast<int>(Direction::Left));
  QCOMPARE(object.value(QStringLiteral("x")).toInt(), -7);
  QCOMPARE(object.value(QStringLiteral("y")).toInt(), 42);
  QCOMPARE(object.value(QStringLiteral("p")).toArray().size(), 1);
  QCOMPARE(object.value(QStringLiteral("p")).toArray().first().toString(), value.paths.first());
}

void FileTransferEdgeDropIpcTests::rejectsInvalidEncodeInputs()
{
  auto value = fixtureValue(Direction::Left);

  value.direction = Direction::NoDirection;
  auto result = deskflow::ipc::encodeFileTransferEdgeDropIpc(value);
  QCOMPARE(result.error, FileTransferEdgeDropIpcError::InvalidDirection);

  value = fixtureValue(Direction::Right);
  value.paths = {QString()};
  result = deskflow::ipc::encodeFileTransferEdgeDropIpc(value);
  QCOMPARE(result.error, FileTransferEdgeDropIpcError::EmptyPath);

  value = fixtureValue(Direction::Top);
  value.paths = {QStringLiteral("relative/path.txt")};
  result = deskflow::ipc::encodeFileTransferEdgeDropIpc(value);
  QCOMPARE(result.error, FileTransferEdgeDropIpcError::RelativePath);

  value = fixtureValue(Direction::Bottom);
  value.paths.clear();
  for (int index = 0; index < 101; ++index) {
    value.paths.append(absoluteNativePath(QStringLiteral("tmp/edge drop/many/file-%1.txt").arg(index)));
  }
  result = deskflow::ipc::encodeFileTransferEdgeDropIpc(value);
  QCOMPARE(result.error, FileTransferEdgeDropIpcError::TooManyPaths);

  value = fixtureValue(Direction::Left);
  value.paths = {
      absoluteNativePath(QStringLiteral("tmp/edge drop/%1").arg(QString(15000, QLatin1Char('a')))),
      absoluteNativePath(QStringLiteral("tmp/edge drop/%1").arg(QString(15000, QLatin1Char('b')))),
      absoluteNativePath(QStringLiteral("tmp/edge drop/%1").arg(QString(15000, QLatin1Char('c')))),
      absoluteNativePath(QStringLiteral("tmp/edge drop/%1").arg(QString(15000, QLatin1Char('d')))),
  };
  result = deskflow::ipc::encodeFileTransferEdgeDropIpc(value);
  QCOMPARE(result.error, FileTransferEdgeDropIpcError::EncodedValueTooLarge);
}

void FileTransferEdgeDropIpcTests::rejectsInvalidDecodeInputs_data()
{
  QTest::addColumn<QString>("encodedValue");
  QTest::addColumn<int>("expectedError");

  const auto absolutePath = absoluteNativePath(QStringLiteral("tmp/edge drop/fixture.txt"));
  const auto windowsStylePath = QStringLiteral(R"(C:\Users\tester\한글 경로\fixture.txt)");
  QTest::newRow("malformed-base64") << QStringLiteral("%not-base64")
                                    << static_cast<int>(FileTransferEdgeDropIpcError::InvalidBase64);
  QTest::newRow("malformed-json") << encodeFixtureJson(QByteArrayLiteral("{"))
                                  << static_cast<int>(FileTransferEdgeDropIpcError::InvalidJson);
  QTest::newRow("non-object-json") << encodeFixtureJson(QByteArrayLiteral("[]"))
                                   << static_cast<int>(FileTransferEdgeDropIpcError::InvalidJson);
  QTest::newRow("unsupported-version") << encodeMutatedJsonObject(QJsonArray{absolutePath}, QStringLiteral("v"), 2)
                                       << static_cast<int>(FileTransferEdgeDropIpcError::InvalidVersion);
  QTest::newRow("unsupported-version-windows-style-path")
      << encodeMutatedJsonObject(QJsonArray{windowsStylePath}, QStringLiteral("v"), 2)
      << static_cast<int>(FileTransferEdgeDropIpcError::InvalidVersion);
  QTest::newRow("invalid-direction") << encodeMutatedJsonObject(QJsonArray{absolutePath}, QStringLiteral("d"), 0)
                                     << static_cast<int>(FileTransferEdgeDropIpcError::InvalidDirection);
  QTest::newRow("non-integral-direction") << encodeMutatedJsonObject(QJsonArray{absolutePath}, QStringLiteral("d"), 1.5)
                                          << static_cast<int>(FileTransferEdgeDropIpcError::InvalidInteger);
  QTest::newRow("x-out-of-range") << encodeMutatedJsonObject(
                                         QJsonArray{absolutePath}, QStringLiteral("x"), 2147483648.0
                                     )
                                  << static_cast<int>(FileTransferEdgeDropIpcError::InvalidInteger);
  QTest::newRow("y-non-integral") << encodeMutatedJsonObject(QJsonArray{absolutePath}, QStringLiteral("y"), -3.25)
                                  << static_cast<int>(FileTransferEdgeDropIpcError::InvalidInteger);
  QTest::newRow("missing-field") << encodeJsonObject(
                                        QJsonObject{
                                            {QStringLiteral("v"), 1},
                                            {QStringLiteral("d"), 1},
                                            {QStringLiteral("x"), 0},
                                            {QStringLiteral("y"), 0},
                                        }
                                    )
                                 << static_cast<int>(FileTransferEdgeDropIpcError::MissingField);
  QTest::newRow("unknown-field") << encodeJsonObject(
                                        QJsonObject{
                                            {QStringLiteral("v"), 1},
                                            {QStringLiteral("d"), 1},
                                            {QStringLiteral("x"), 0},
                                            {QStringLiteral("y"), 0},
                                            {QStringLiteral("p"), QJsonArray{absolutePath}},
                                            {QStringLiteral("extra"), true},
                                        }
                                    )
                                 << static_cast<int>(FileTransferEdgeDropIpcError::UnknownField);
  QTest::newRow("paths-not-array") << encodeJsonObject(
                                          QJsonObject{
                                              {QStringLiteral("v"), 1},
                                              {QStringLiteral("d"), 1},
                                              {QStringLiteral("x"), 0},
                                              {QStringLiteral("y"), 0},
                                              {QStringLiteral("p"), absolutePath},
                                          }
                                      )
                                   << static_cast<int>(FileTransferEdgeDropIpcError::InvalidFieldType);
  QTest::newRow("empty-path") << encodeJsonObject(validJsonObject(QJsonArray{QString{}}))
                              << static_cast<int>(FileTransferEdgeDropIpcError::EmptyPath);
  QTest::newRow("relative-path") << encodeJsonObject(validJsonObject(QJsonArray{QStringLiteral("relative/path.txt")}))
                                 << static_cast<int>(FileTransferEdgeDropIpcError::RelativePath);
  QTest::newRow("non-string-path") << encodeJsonObject(validJsonObject(QJsonArray{17}))
                                   << static_cast<int>(FileTransferEdgeDropIpcError::InvalidPathType);
  QJsonArray tooManyPaths;
  for (int index = 0; index < 101; ++index) {
    tooManyPaths.append(absoluteNativePath(QStringLiteral("tmp/edge drop/many/file-%1.txt").arg(index)));
  }
  QTest::newRow("too-many-paths") << encodeJsonObject(
                                         QJsonObject{
                                             {QStringLiteral("v"), 1},
                                             {QStringLiteral("d"), 1},
                                             {QStringLiteral("x"), 0},
                                             {QStringLiteral("y"), 0},
                                             {QStringLiteral("p"), tooManyPaths},
                                         }
                                     )
                                  << static_cast<int>(FileTransferEdgeDropIpcError::TooManyPaths);
  QTest::newRow("encoded-value-too-large")
      << QString(61441, QLatin1Char('A')) << static_cast<int>(FileTransferEdgeDropIpcError::EncodedValueTooLarge);
}

void FileTransferEdgeDropIpcTests::rejectsInvalidDecodeInputs()
{
  QFETCH(QString, encodedValue);
  QFETCH(int, expectedError);

  const auto decoded = deskflow::ipc::decodeFileTransferEdgeDropIpc(encodedValue);
  QCOMPARE(decoded.error, static_cast<FileTransferEdgeDropIpcError>(expectedError));
  QVERIFY(!decoded.value.has_value());
}

void FileTransferEdgeDropIpcTests::sanitizesSensitiveEdgeDropMessages()
{
  QCOMPARE(
      deskflow::ipc::sanitizeMessageForLog(QStringLiteral("fileTransferEdgeDrop=payload")),
      QStringLiteral("fileTransferEdgeDrop=<redacted>")
  );
  QCOMPARE(
      deskflow::ipc::sanitizeMessageForLog(QStringLiteral("fileTransferEdgeDrop=")),
      QStringLiteral("fileTransferEdgeDrop=<redacted>")
  );
  QCOMPARE(
      deskflow::ipc::sanitizeMessageForLog(QStringLiteral("fileTransferEdgeDrop")),
      QStringLiteral("fileTransferEdgeDrop=<redacted>")
  );
  QCOMPARE(
      deskflow::ipc::sanitizeMessageForLog(QStringLiteral("fileTransferEdgeDropExtra=payload")),
      QStringLiteral("fileTransferEdgeDropExtra=payload")
  );
  QCOMPARE(deskflow::ipc::sanitizeMessageForLog(QStringLiteral("hello=world")), QStringLiteral("hello=world"));
}

QTEST_MAIN(FileTransferEdgeDropIpcTests)

#include "FileTransferEdgeDropIpcTests.moc"
