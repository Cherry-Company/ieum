/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferSessionState.h"

#include "deskflow/FileTransferOfferValidator.h"

#include <utility>

namespace deskflow::filetransfer {

namespace {

FileTransferSessionMutation failure(FileTransferSessionError error)
{
  return {.error = error};
}

FileTransferDecisionCreation decisionFailure(FileTransferSessionError error)
{
  return {.error = error, .decision = std::nullopt};
}

bool validDecision(FileTransferDecisionValue value)
{
  return value == FileTransferDecisionValue::Accept || value == FileTransferDecisionValue::Reject;
}

bool validCancelReason(FileTransferCancelReason value)
{
  return value >= FileTransferCancelReason::SenderCancelled && value <= FileTransferCancelReason::Disconnected;
}

bool validResult(FileTransferResultStatus status, FileTransferResultCode code)
{
  const auto validStatus =
      status >= FileTransferResultStatus::Completed && status <= FileTransferResultStatus::Cancelled;
  const auto validCode = code >= FileTransferResultCode::None && code <= FileTransferResultCode::ProtocolError;
  if (!validStatus || !validCode) {
    return false;
  }
  if (status == FileTransferResultStatus::Completed) {
    return code == FileTransferResultCode::None;
  }
  if (status == FileTransferResultStatus::Failed) {
    return code != FileTransferResultCode::None;
  }
  return code == FileTransferResultCode::None;
}

bool validSourceManifest(const FileTransferSourceManifest &manifest)
{
  if (!validateOffer(manifest.offer).ok() || manifest.sources.size() != manifest.offer.items.size()) {
    return false;
  }

  for (std::size_t position = 0; position < manifest.sources.size(); ++position) {
    const auto &source = manifest.sources[position];
    const auto &item = manifest.offer.items[position];
    if (source.path.empty() || !source.path.is_absolute() || source.identity.length == 0 ||
        source.identity.length > source.identity.bytes.size() || source.name != item.name || source.size != item.size) {
      return false;
    }
  }
  return true;
}

} // namespace

FileTransferSessionRegistry::FileTransferSessionRegistry(std::string localScreen, std::size_t maxConcurrentSessions)
    : m_localScreen(std::move(localScreen)),
      m_maxConcurrentSessions(maxConcurrentSessions)
{
}

FileTransferSessionMutation FileTransferSessionRegistry::registerOutgoing(FileTransferSourceManifest manifest)
{
  if (!validSourceManifest(manifest)) {
    return failure(FileTransferSessionError::InvalidMessage);
  }
  if (manifest.offer.sourceScreen != m_localScreen) {
    return failure(FileTransferSessionError::WrongLocalRole);
  }
  if (contains(manifest.offer.id)) {
    return failure(FileTransferSessionError::DuplicateTransfer);
  }
  if (m_sessions.size() >= m_maxConcurrentSessions) {
    return failure(FileTransferSessionError::CapacityExceeded);
  }

  const auto id = manifest.offer.id;
  const auto storedOffer = manifest.offer;
  m_sessions.emplace(
      id,
      FileTransferSessionRecord{
          .offer = storedOffer,
          .role = FileTransferSessionRole::Sender,
          .phase = FileTransferSessionPhase::AwaitingDecision,
          .sourceManifest = std::move(manifest),
      }
  );
  return {};
}

FileTransferSessionMutation FileTransferSessionRegistry::registerIncoming(FileTransferOffer offer)
{
  if (!validateOffer(offer).ok()) {
    return failure(FileTransferSessionError::InvalidMessage);
  }
  if (contains(offer.id)) {
    return failure(FileTransferSessionError::DuplicateTransfer);
  }
  if (offer.targetScreen != m_localScreen) {
    return failure(FileTransferSessionError::WrongLocalRole);
  }
  if (m_sessions.size() >= m_maxConcurrentSessions) {
    return failure(FileTransferSessionError::CapacityExceeded);
  }

  const auto id = offer.id;
  m_sessions.emplace(
      id,
      FileTransferSessionRecord{
          .offer = std::move(offer),
          .role = FileTransferSessionRole::Receiver,
          .phase = FileTransferSessionPhase::AwaitingDecision,
          .sourceManifest = std::nullopt,
      }
  );
  return {};
}

FileTransferDecisionCreation
FileTransferSessionRegistry::decideIncoming(TransferId id, FileTransferDecisionValue decision)
{
  if (!validDecision(decision)) {
    return decisionFailure(FileTransferSessionError::InvalidMessage);
  }
  auto *session = findMutable(id);
  if (session == nullptr) {
    return decisionFailure(FileTransferSessionError::UnknownTransfer);
  }
  if (session->role != FileTransferSessionRole::Receiver) {
    return decisionFailure(FileTransferSessionError::WrongLocalRole);
  }
  if (session->phase != FileTransferSessionPhase::AwaitingDecision) {
    return decisionFailure(FileTransferSessionError::WrongPhase);
  }

  session->phase = decision == FileTransferDecisionValue::Accept ? FileTransferSessionPhase::AwaitingData
                                                                 : FileTransferSessionPhase::Rejected;
  return {
      .error = FileTransferSessionError::None,
      .decision =
          FileTransferDecision{
              .id = id,
              .sourceScreen = session->offer.sourceScreen,
              .targetScreen = session->offer.targetScreen,
              .decision = decision,
          },
  };
}

FileTransferSessionMutation FileTransferSessionRegistry::applyDecision(const FileTransferDecision &decision)
{
  if (!validDecision(decision.decision)) {
    return failure(FileTransferSessionError::InvalidMessage);
  }
  auto *session = findMutable(decision.id);
  if (session == nullptr) {
    return failure(FileTransferSessionError::UnknownTransfer);
  }
  if (session->role != FileTransferSessionRole::Sender) {
    return failure(FileTransferSessionError::WrongLocalRole);
  }
  if (!routeMatches(*session, decision.id, decision.sourceScreen, decision.targetScreen)) {
    return failure(FileTransferSessionError::RouteMismatch);
  }
  if (session->phase != FileTransferSessionPhase::AwaitingDecision) {
    return failure(FileTransferSessionError::WrongPhase);
  }

  session->phase = decision.decision == FileTransferDecisionValue::Accept ? FileTransferSessionPhase::ReadyToSend
                                                                          : FileTransferSessionPhase::Rejected;
  return {};
}

FileTransferSessionMutation FileTransferSessionRegistry::applyCancel(const FileTransferCancel &cancel)
{
  if (!validCancelReason(cancel.reason)) {
    return failure(FileTransferSessionError::InvalidMessage);
  }
  auto *session = findMutable(cancel.id);
  if (session == nullptr) {
    return failure(FileTransferSessionError::UnknownTransfer);
  }
  if (!routeMatches(*session, cancel.id, cancel.sourceScreen, cancel.targetScreen)) {
    return failure(FileTransferSessionError::RouteMismatch);
  }
  if (isTerminal(session->phase)) {
    return failure(FileTransferSessionError::WrongPhase);
  }

  session->phase = FileTransferSessionPhase::Cancelled;
  return {};
}

FileTransferSessionMutation FileTransferSessionRegistry::applyResult(const FileTransferResult &result)
{
  if (!validResult(result.status, result.code)) {
    return failure(FileTransferSessionError::InvalidMessage);
  }
  auto *session = findMutable(result.id);
  if (session == nullptr) {
    return failure(FileTransferSessionError::UnknownTransfer);
  }
  if (!routeMatches(*session, result.id, result.sourceScreen, result.targetScreen)) {
    return failure(FileTransferSessionError::RouteMismatch);
  }
  if (isTerminal(session->phase)) {
    return failure(FileTransferSessionError::WrongPhase);
  }
  if (result.status == FileTransferResultStatus::Completed && session->phase != FileTransferSessionPhase::Sending &&
      session->phase != FileTransferSessionPhase::Receiving) {
    return failure(FileTransferSessionError::WrongPhase);
  }

  switch (result.status) {
  case FileTransferResultStatus::Completed:
    session->phase = FileTransferSessionPhase::Completed;
    break;
  case FileTransferResultStatus::Failed:
    session->phase = FileTransferSessionPhase::Failed;
    break;
  case FileTransferResultStatus::Cancelled:
    session->phase = FileTransferSessionPhase::Cancelled;
    break;
  }
  return {};
}

FileTransferSessionMutation FileTransferSessionRegistry::beginTransfer(const TransferId &id)
{
  auto *session = findMutable(id);
  if (session == nullptr) {
    return failure(FileTransferSessionError::UnknownTransfer);
  }
  if (session->role == FileTransferSessionRole::Sender && session->phase == FileTransferSessionPhase::ReadyToSend) {
    session->phase = FileTransferSessionPhase::Sending;
    return {};
  }
  if (session->role == FileTransferSessionRole::Receiver && session->phase == FileTransferSessionPhase::AwaitingData) {
    session->phase = FileTransferSessionPhase::Receiving;
    return {};
  }
  return failure(FileTransferSessionError::WrongPhase);
}

const FileTransferSessionRecord *FileTransferSessionRegistry::find(const TransferId &id) const noexcept
{
  const auto found = m_sessions.find(id);
  return found == m_sessions.end() ? nullptr : &found->second;
}

FileTransferSessionRecord *FileTransferSessionRegistry::findMutable(const TransferId &id) noexcept
{
  const auto found = m_sessions.find(id);
  return found == m_sessions.end() ? nullptr : &found->second;
}

bool FileTransferSessionRegistry::eraseTerminal(const TransferId &id)
{
  const auto found = m_sessions.find(id);
  if (found == m_sessions.end() || !isTerminal(found->second.phase)) {
    return false;
  }
  m_sessions.erase(found);
  return true;
}

std::size_t FileTransferSessionRegistry::size() const noexcept
{
  return m_sessions.size();
}

bool FileTransferSessionRegistry::contains(const TransferId &id) const
{
  return m_sessions.contains(id);
}

bool FileTransferSessionRegistry::routeMatches(
    const FileTransferSessionRecord &session, const TransferId &id, const std::string &source, const std::string &target
) const
{
  return session.offer.id == id && session.offer.sourceScreen == source && session.offer.targetScreen == target;
}

bool FileTransferSessionRegistry::isTerminal(FileTransferSessionPhase phase) noexcept
{
  return phase == FileTransferSessionPhase::Rejected || phase == FileTransferSessionPhase::Completed ||
         phase == FileTransferSessionPhase::Failed || phase == FileTransferSessionPhase::Cancelled;
}

} // namespace deskflow::filetransfer
