/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"
#include "deskflow/FileTransferControlCodec.h"

#include <utility>

namespace deskflow::filetransfer {

class FileTransferControlEventData final : public EventData
{
public:
  explicit FileTransferControlEventData(FileTransferControlMessage message) : m_message(std::move(message))
  {
  }

  [[nodiscard]] const FileTransferControlMessage &message() const noexcept
  {
    return m_message;
  }

private:
  FileTransferControlMessage m_message;
};

} // namespace deskflow::filetransfer
