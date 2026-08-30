/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferSessionState.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

using namespace deskflow::filetransfer;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

TransferId transferId(std::uint8_t value)
{
  TransferId id{};
  id[0] = value;
  return id;
}

FileTransferOffer offer(std::uint8_t id = 1, std::string source = "office", std::string target = "laptop")
{
  return {
      .id = transferId(id),
      .sourceScreen = std::move(source),
      .targetScreen = std::move(target),
      .items = {{.index = 0, .name = "report.txt", .size = 7}},
  };
}

FileTransferSourceManifest manifest(std::uint8_t id = 1)
{
  FileTransferSourceCandidate source;
  source.path = std::filesystem::current_path() / "report.txt";
  source.identity.length = 2;
  source.identity.bytes[0] = 0x42;
  source.identity.bytes[1] = 0x24;
  source.size = 7;
  source.lastWriteTime = 1234;
  source.name = "report.txt";
  return {.offer = offer(id), .sources = {std::move(source)}};
}

void outgoingAcceptPreservesSourceSnapshot()
{
  FileTransferSessionRegistry sessions("office");
  auto source = manifest();
  const auto expectedPath = source.sources.front().path;

  require(sessions.registerOutgoing(std::move(source)).ok(), "outgoing manifest should register");
  const auto *registered = sessions.find(transferId(1));
  require(registered != nullptr, "outgoing session should be findable");
  require(registered->role == FileTransferSessionRole::Sender, "outgoing session should have sender role");
  require(registered->phase == FileTransferSessionPhase::AwaitingDecision, "outgoing session should await a decision");
  require(
      registered->sourceManifest.has_value() && registered->sourceManifest->sources.front().path == expectedPath,
      "outgoing session should retain the checked source path"
  );

  const FileTransferDecision accepted{
      .id = transferId(1),
      .sourceScreen = "office",
      .targetScreen = "laptop",
      .decision = FileTransferDecisionValue::Accept,
  };
  require(sessions.applyDecision(accepted).ok(), "target acceptance should apply at source");
  require(
      sessions.find(transferId(1))->phase == FileTransferSessionPhase::ReadyToSend,
      "acceptance should make the source ready"
  );
  require(sessions.beginTransfer(transferId(1)).ok(), "ready source should begin transfer");
  require(
      sessions.find(transferId(1))->phase == FileTransferSessionPhase::Sending, "source should enter sending phase"
  );
}

void incomingDecisionIsRoutedOnce()
{
  FileTransferSessionRegistry sessions("laptop");
  require(sessions.registerIncoming(offer()).ok(), "incoming offer should register at its target");

  const auto accepted = sessions.decideIncoming(transferId(1), FileTransferDecisionValue::Accept);
  require(accepted.ok() && accepted.decision.has_value(), "incoming acceptance should create a decision");
  require(
      *accepted.decision ==
          FileTransferDecision{
              .id = transferId(1),
              .sourceScreen = "office",
              .targetScreen = "laptop",
              .decision = FileTransferDecisionValue::Accept,
          },
      "decision should preserve the original route"
  );
  require(
      sessions.find(transferId(1))->phase == FileTransferSessionPhase::AwaitingData, "accepted target should await data"
  );
  require(
      sessions.decideIncoming(transferId(1), FileTransferDecisionValue::Reject).error ==
          FileTransferSessionError::WrongPhase,
      "incoming decision should be one-shot"
  );
  require(sessions.beginTransfer(transferId(1)).ok(), "accepted target should begin receiving");
  require(
      sessions.find(transferId(1))->phase == FileTransferSessionPhase::Receiving, "target should enter receiving phase"
  );
}

void rejectsDuplicatesWrongRolesAndSpoofedRoutes()
{
  FileTransferSessionRegistry sourceSessions("office");
  require(sourceSessions.registerOutgoing(manifest()).ok(), "setup outgoing session should register");
  require(
      sourceSessions.registerOutgoing(manifest()).error == FileTransferSessionError::DuplicateTransfer,
      "duplicate outgoing transfer ID should be rejected"
  );
  require(
      sourceSessions.registerIncoming(offer()).error == FileTransferSessionError::DuplicateTransfer,
      "incoming ID must not collide with outgoing ID"
  );
  require(
      sourceSessions.registerIncoming(offer(2)).error == FileTransferSessionError::WrongLocalRole,
      "source screen should not register an offer addressed elsewhere"
  );

  FileTransferDecision spoofed{
      .id = transferId(1),
      .sourceScreen = "office",
      .targetScreen = "attacker",
      .decision = FileTransferDecisionValue::Accept,
  };
  require(
      sourceSessions.applyDecision(spoofed).error == FileTransferSessionError::RouteMismatch,
      "decision route spoofing should be rejected"
  );
  spoofed.id = transferId(99);
  require(
      sourceSessions.applyDecision(spoofed).error == FileTransferSessionError::UnknownTransfer,
      "unknown decision should be rejected"
  );
}

void cancellationResultsAndCleanupAreTerminal()
{
  FileTransferSessionRegistry sessions("office");
  require(sessions.registerOutgoing(manifest()).ok(), "setup outgoing session should register");

  const FileTransferCancel cancelled{
      .id = transferId(1),
      .sourceScreen = "office",
      .targetScreen = "laptop",
      .reason = FileTransferCancelReason::ReceiverCancelled,
  };
  require(sessions.applyCancel(cancelled).ok(), "participant cancellation should apply");
  require(
      sessions.find(transferId(1))->phase == FileTransferSessionPhase::Cancelled, "cancellation should become terminal"
  );
  require(
      sessions.applyCancel(cancelled).error == FileTransferSessionError::WrongPhase,
      "terminal session should reject a second cancellation"
  );
  require(sessions.eraseTerminal(transferId(1)), "terminal session should erase");
  require(sessions.find(transferId(1)) == nullptr, "erase should release the source snapshot");

  require(sessions.registerOutgoing(manifest(2)).ok(), "second outgoing session should register");
  const FileTransferResult completed{
      .id = transferId(2),
      .sourceScreen = "office",
      .targetScreen = "laptop",
      .status = FileTransferResultStatus::Completed,
      .code = FileTransferResultCode::None,
  };
  require(
      sessions.applyResult(completed).error == FileTransferSessionError::WrongPhase,
      "completion before data starts should be rejected"
  );
  auto invalidFailure = completed;
  invalidFailure.status = FileTransferResultStatus::Failed;
  require(
      sessions.applyResult(invalidFailure).error == FileTransferSessionError::InvalidMessage,
      "failed result should carry a concrete error code"
  );
  const FileTransferDecision accepted{
      .id = transferId(2),
      .sourceScreen = "office",
      .targetScreen = "laptop",
      .decision = FileTransferDecisionValue::Accept,
  };
  require(sessions.applyDecision(accepted).ok(), "second outgoing session should accept");
  require(sessions.beginTransfer(transferId(2)).ok(), "second outgoing session should begin");
  require(sessions.applyResult(completed).ok(), "completion result should apply");
  require(
      sessions.find(transferId(2))->phase == FileTransferSessionPhase::Completed,
      "completed result should become terminal"
  );
}

void enforcesCapacityAndRejectsInvalidEnums()
{
  FileTransferSessionRegistry sessions("laptop", 1);
  require(sessions.registerIncoming(offer()).ok(), "first incoming session should fit");
  require(
      sessions.registerIncoming(offer(2)).error == FileTransferSessionError::CapacityExceeded,
      "session cap should fail closed"
  );
  require(
      sessions.decideIncoming(transferId(1), static_cast<FileTransferDecisionValue>(99)).error ==
          FileTransferSessionError::InvalidMessage,
      "unknown decision enum should be rejected"
  );
  require(!sessions.eraseTerminal(transferId(1)), "non-terminal session should not erase through terminal API");
}

} // namespace

int main()
{
  outgoingAcceptPreservesSourceSnapshot();
  incomingDecisionIsRoutedOnce();
  rejectsDuplicatesWrongRolesAndSpoofedRoutes();
  cancellationResultsAndCleanupAreTerminal();
  enforcesCapacityAndRejectsInvalidEnums();
  std::cout << "PASS: FileTransferSessionStateTests\n";
  return 0;
}
