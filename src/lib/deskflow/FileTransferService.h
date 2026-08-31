/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferControlCodec.h"
#include "deskflow/FileTransferDataCodec.h"
#include "deskflow/FileTransferPlatform.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class IEventQueue;

namespace deskflow::filetransfer {

struct FileTransferServiceOptions
{
  using AuthorizationCheck = std::function<bool()>;
  using ControlSender = std::function<bool(const FileTransferControlMessage &)>;
  using DataSender = std::function<bool(const FileTransferDataMessage &)>;

  std::string localScreen;
  std::filesystem::path destinationDirectory;
  bool receiveEnabled = false;
  AuthorizationCheck authorizeFileTransfer;
  ControlSender sendControl;
  DataSender sendData;
  std::unique_ptr<IFileTransferPlatform> platform;
  IEventQueue *events = nullptr;
  void *eventTarget = nullptr;
};

class FileTransferService final
{
public:
  explicit FileTransferService(FileTransferServiceOptions options);
  ~FileTransferService();

  FileTransferService(const FileTransferService &) = delete;
  FileTransferService &operator=(const FileTransferService &) = delete;

  [[nodiscard]] bool offerLocalFiles(std::string targetScreen, std::vector<FileTransferSourceCandidate> candidates);
  [[nodiscard]] bool handleControl(const FileTransferControlMessage &message);
  [[nodiscard]] bool handleData(const FileTransferDataMessage &message);
  [[nodiscard]] bool processNextOutgoingChunk();
  [[nodiscard]] std::size_t activeSessionCount() const noexcept;

private:
  struct Impl;

  void scheduleNextOutgoingChunk();
  [[nodiscard]] bool handleOffer(const FileTransferOffer &offer);
  [[nodiscard]] bool handleDecision(const FileTransferDecision &decision);
  [[nodiscard]] bool handleCancel(const FileTransferCancel &cancel);
  [[nodiscard]] bool handleResult(const FileTransferResult &result);
  [[nodiscard]] bool failSession(const TransferId &id, FileTransferResultCode code, bool notifyPeer);

  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::filetransfer
