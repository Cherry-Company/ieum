/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferControlCodec.h"
#include "deskflow/FileTransferSourceManifest.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>

namespace deskflow::filetransfer {

enum class FileTransferSessionRole
{
  Sender,
  Receiver,
};

enum class FileTransferSessionPhase
{
  AwaitingDecision,
  ReadyToSend,
  AwaitingData,
  Sending,
  Receiving,
  Rejected,
  Completed,
  Failed,
  Cancelled,
};

enum class FileTransferSessionError
{
  None,
  InvalidMessage,
  DuplicateTransfer,
  UnknownTransfer,
  WrongLocalRole,
  RouteMismatch,
  WrongPhase,
  CapacityExceeded,
};

struct FileTransferSessionMutation
{
  FileTransferSessionError error = FileTransferSessionError::None;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferSessionError::None;
  }
};

struct FileTransferDecisionCreation
{
  FileTransferSessionError error = FileTransferSessionError::None;
  std::optional<FileTransferDecision> decision;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferSessionError::None;
  }
};

struct FileTransferSessionRecord
{
  FileTransferOffer offer;
  FileTransferSessionRole role = FileTransferSessionRole::Sender;
  FileTransferSessionPhase phase = FileTransferSessionPhase::AwaitingDecision;
  std::optional<FileTransferSourceManifest> sourceManifest;
};

class FileTransferSessionRegistry final
{
public:
  explicit FileTransferSessionRegistry(std::string localScreen, std::size_t maxConcurrentSessions = 16);

  [[nodiscard]] FileTransferSessionMutation registerOutgoing(FileTransferSourceManifest manifest);
  [[nodiscard]] FileTransferSessionMutation registerIncoming(FileTransferOffer offer);
  [[nodiscard]] FileTransferDecisionCreation decideIncoming(TransferId id, FileTransferDecisionValue decision);

  [[nodiscard]] FileTransferSessionMutation applyDecision(const FileTransferDecision &decision);
  [[nodiscard]] FileTransferSessionMutation applyCancel(const FileTransferCancel &cancel);
  [[nodiscard]] FileTransferSessionMutation applyResult(const FileTransferResult &result);
  [[nodiscard]] FileTransferSessionMutation beginTransfer(const TransferId &id);

  [[nodiscard]] const FileTransferSessionRecord *find(const TransferId &id) const noexcept;
  [[nodiscard]] bool eraseTerminal(const TransferId &id);
  [[nodiscard]] std::size_t size() const noexcept;

private:
  [[nodiscard]] FileTransferSessionRecord *findMutable(const TransferId &id) noexcept;
  [[nodiscard]] bool contains(const TransferId &id) const;
  [[nodiscard]] bool routeMatches(
      const FileTransferSessionRecord &session, const TransferId &id, const std::string &source,
      const std::string &target
  ) const;
  [[nodiscard]] static bool isTerminal(FileTransferSessionPhase phase) noexcept;

  std::string m_localScreen;
  std::size_t m_maxConcurrentSessions = 0;
  std::map<TransferId, FileTransferSessionRecord> m_sessions;
};

} // namespace deskflow::filetransfer
