/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"
#include "deskflow/FileTransferDataCodec.h"

#include <utility>

namespace deskflow::filetransfer {

class FileTransferDataEventData final : public EventData
{
public:
  explicit FileTransferDataEventData(FileTransferDataMessage message) : message(std::move(message))
  {
  }

  FileTransferDataMessage message;
};

} // namespace deskflow::filetransfer
