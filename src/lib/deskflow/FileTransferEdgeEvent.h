/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"
#include "deskflow/FileTransferEdgeCodec.h"

#include <utility>

namespace deskflow::filetransfer {

class FileTransferEdgeEventData final : public EventData
{
public:
  explicit FileTransferEdgeEventData(FileTransferEdgeMessage message) : m_message(std::move(message))
  {
  }

  [[nodiscard]] const FileTransferEdgeMessage &message() const noexcept
  {
    return m_message;
  }

private:
  FileTransferEdgeMessage m_message;
};

} // namespace deskflow::filetransfer
