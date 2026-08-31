/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferControlCodec.h"
#include "deskflow/FileTransferDataCodec.h"
#include "deskflow/FileTransferSourceManifest.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class IEventQueue;

namespace deskflow::filetransfer {

struct MSWindowsFileTransferServiceOptions
{
  using ControlSender = std::function<bool(const FileTransferControlMessage &)>;
  using DataSender = std::function<bool(const FileTransferDataMessage &)>;
  using TransferIdGenerator = std::function<TransferId()>;
  using AuthorizationCheck = std::function<bool()>;

  std::string localScreen;
  std::filesystem::path destinationDirectory;
  bool receiveEnabled = false;
  AuthorizationCheck authorizeFileTransfer;
  ControlSender sendControl;
  DataSender sendData;
  TransferIdGenerator createTransferId;
  IEventQueue *events = nullptr;
  void *eventTarget = nullptr;
};

class MSWindowsFileTransferService final
{
public:
  explicit MSWindowsFileTransferService(MSWindowsFileTransferServiceOptions options);
  ~MSWindowsFileTransferService();

  MSWindowsFileTransferService(const MSWindowsFileTransferService &) = delete;
  MSWindowsFileTransferService &operator=(const MSWindowsFileTransferService &) = delete;

  [[nodiscard]] bool offerLocalFiles(std::string targetScreen, std::vector<FileTransferSourceCandidate> candidates);
  [[nodiscard]] bool handleControl(const FileTransferControlMessage &message);
  [[nodiscard]] bool handleData(const FileTransferDataMessage &message);

  //! Process at most one source chunk. Production schedules this on the event loop.
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
