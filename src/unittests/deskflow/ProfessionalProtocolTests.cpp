// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "deskflow/ProfessionalProtocol.h"
#include "io/IStream.h"
#include <QTest>
#include <QtEndian>
#include <algorithm>
#include <cstring>

using namespace deskflow::professional;
class FragmentedStream final : public deskflow::IStream
{
public:
  QByteArray bytes, written;
  uint32_t largestRead = 0, fragment = 3;
  void close() override
  {
  }
  uint32_t read(void *out, uint32_t size) override
  {
    largestRead = std::max(largestRead, size);
    const auto count = std::min({size, fragment, static_cast<uint32_t>(bytes.size())});
    std::memcpy(out, bytes.constData(), count);
    bytes.remove(0, count);
    return count;
  }
  void write(const void *data, uint32_t size) override
  {
    written.append(static_cast<const char *>(data), size);
  }
  void flush() override
  {
  }
  void shutdownInput() override
  {
  }
  void shutdownOutput() override
  {
  }
  void *getEventTarget() const override
  {
    return const_cast<FragmentedStream *>(this);
  }
  bool isReady() const override
  {
    return !bytes.isEmpty();
  }
  uint32_t getSize() const override
  {
    return static_cast<uint32_t>(bytes.size());
  }
};
class ProfessionalProtocolTests final : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void transportPreservesBinaryPayloadThroughFragmentedReads()
  {
    FragmentedStream stream;
    const Message expected{
        "source",
        "target",
        "audio",
        "audio-sharing",
        "c78e37de-8b03-40b5-80b7-ff6e74073043",
        {{"seq", 1}, {"data", QString::fromLatin1(QByteArray(kChunkBytes, '\xff').toBase64())}}
    };
    QVERIFY(writeFrame(&stream, expected));
    QVERIFY(stream.written.startsWith("DPRF"));
    stream.bytes = stream.written.mid(4) + "NEXT";
    const auto decoded = readFrame(&stream);
    QVERIFY(decoded);
    QCOMPARE(encode(*decoded), encode(expected));
    QCOMPARE(stream.bytes, QByteArray("NEXT"));
  }
  void rejectsOversizedLengthBeforeReadingPayload()
  {
    for (quint32 length : {0U, static_cast<quint32>(kMaxMessageBytes + 1), 0xffffffffU}) {
      FragmentedStream stream;
      const auto header = qToBigEndian(length);
      stream.bytes = QByteArray(reinterpret_cast<const char *>(&header), 4) + "DO-NOT-READ";
      QVERIFY(!readFrame(&stream));
      QCOMPARE(stream.largestRead, 4U);
      QCOMPARE(stream.bytes, QByteArray("DO-NOT-READ"));
    }
  }
  void rejectsEveryTruncatedEnvelopeAndMalformedJson()
  {
    FragmentedStream stream;
    QVERIFY(writeFrame(&stream, {{}, {}, "discover", {}, {}, {}}));
    const auto complete = stream.written.mid(4);
    for (qsizetype size = 0; size < complete.size(); ++size) {
      stream.bytes = complete.left(size);
      QVERIFY(!readFrame(&stream));
    }
    const auto header = qToBigEndian(2U);
    stream.bytes = QByteArray(reinterpret_cast<const char *>(&header), 4) + "[]";
    QVERIFY(!readFrame(&stream));
    QVERIFY(!readFrame(nullptr));
    QVERIFY(!writeFrame(nullptr, {{}, {}, "discover", {}, {}, {}}));
    stream.written.clear();
    QVERIFY(!writeFrame(&stream, {{}, {}, "execute", {}, {}, {}}));
    QVERIFY(stream.written.isEmpty());
  }
};
QTEST_GUILESS_MAIN(ProfessionalProtocolTests)
#include "ProfessionalProtocolTests.moc"
