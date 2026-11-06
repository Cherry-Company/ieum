// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "ProfessionalProtocol.h"
#include "io/IStream.h"
#include <QtEndian>

namespace deskflow::professional {
bool writeFrame(IStream *stream, const Message &message)
{
  const auto bytes = encode(message);
  if (!stream || bytes.isEmpty())
    return false;
  QByteArray frame("DPRF", 4);
  const auto size = qToBigEndian(static_cast<quint32>(bytes.size()));
  frame.append(reinterpret_cast<const char *>(&size), sizeof(size));
  frame.append(bytes);
  stream->write(frame.constData(), static_cast<uint32_t>(frame.size()));
  return true;
}

std::optional<Message> readFrame(IStream *stream)
{
  const auto read = [stream](char *output, quint32 size) {
    while (size > 0) {
      const auto count = stream->read(output, size);
      if (count == 0 || count > size)
        return false;
      size -= count;
      output += count;
    }
    return true;
  };
  quint32 size = 0;
  if (!stream || !read(reinterpret_cast<char *>(&size), sizeof(size)))
    return std::nullopt;
  size = qFromBigEndian(size);
  if (size == 0 || size > kMaxMessageBytes)
    return std::nullopt;
  QByteArray bytes(size, Qt::Uninitialized);
  return read(bytes.data(), size) ? decode(bytes) : std::nullopt;
}
} // namespace deskflow::professional
