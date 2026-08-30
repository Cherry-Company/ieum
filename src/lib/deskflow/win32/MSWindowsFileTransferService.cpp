/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/win32/MSWindowsFileTransferService.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/FileTransferControlEvent.h"
#include "deskflow/FileTransferDataEvent.h"
#include "deskflow/FileTransferSessionState.h"
#include "platform/MSWindowsFileTransferIo.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <map>
#include <optional>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

namespace deskflow::filetransfer {
namespace {

TransferId createRandomTransferId()
{
  TransferId id{};
  const auto status =
      BCryptGenRandom(nullptr, id.data(), static_cast<ULONG>(id.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (status != 0 || std::ranges::all_of(id, [](std::uint8_t value) { return value == 0; })) {
    return {};
  }
  return id;
}

FileTransferResultCode sourceResultCodeFor(MSWindowsFileTransferIoError error)
{
  switch (error) {
  case MSWindowsFileTransferIoError::InvalidSource:
  case MSWindowsFileTransferIoError::OpenFailed:
  case MSWindowsFileTransferIoError::ReadFailed:
  case MSWindowsFileTransferIoError::SourceChanged:
    return FileTransferResultCode::SourceChanged;
  case MSWindowsFileTransferIoError::HashFailed:
    return FileTransferResultCode::IntegrityMismatch;
  default:
    return FileTransferResultCode::ProtocolError;
  }
}

FileTransferResultCode destinationResultCodeFor(MSWindowsFileTransferIoError error)
{
  switch (error) {
  case MSWindowsFileTransferIoError::InvalidDestination:
  case MSWindowsFileTransferIoError::DestinationDenied:
  case MSWindowsFileTransferIoError::OpenFailed:
  case MSWindowsFileTransferIoError::WriteFailed:
  case MSWindowsFileTransferIoError::PublishFailed:
    return FileTransferResultCode::DestinationDenied;
  case MSWindowsFileTransferIoError::IntegrityMismatch:
  case MSWindowsFileTransferIoError::HashFailed:
    return FileTransferResultCode::IntegrityMismatch;
  default:
    return FileTransferResultCode::ProtocolError;
  }
}

const FileTransferDataRoute &routeOf(const FileTransferDataMessage &message)
{
  return std::visit([](const auto &value) -> const FileTransferDataRoute & { return value.route; }, message);
}

} // namespace

struct MSWindowsFileTransferService::Impl
{
  struct Outgoing
  {
    TransferId id{};
    std::uint32_t itemIndex = 0;
    std::unique_ptr<MSWindowsFileTransferReader> reader;
  };

  explicit Impl(MSWindowsFileTransferServiceOptions options)
      : localScreen(std::move(options.localScreen)),
        destinationDirectory(std::move(options.destinationDirectory)),
        receiveEnabled(options.receiveEnabled),
        sendControl(std::move(options.sendControl)),
        sendData(std::move(options.sendData)),
        createTransferId(std::move(options.createTransferId)),
        events(options.events),
        eventTarget(options.eventTarget),
        sessions(localScreen, 1)
  {
    if (!createTransferId) {
      createTransferId = createRandomTransferId;
    }
  }

  std::string localScreen;
  std::filesystem::path destinationDirectory;
  bool receiveEnabled = false;
  MSWindowsFileTransferServiceOptions::ControlSender sendControl;
  MSWindowsFileTransferServiceOptions::DataSender sendData;
  MSWindowsFileTransferServiceOptions::TransferIdGenerator createTransferId;
  IEventQueue *events = nullptr;
  void *eventTarget = nullptr;
  FileTransferSessionRegistry sessions;
  std::optional<Outgoing> outgoing;
  std::map<TransferId, std::unique_ptr<MSWindowsFileTransferWriter>> incoming;
  bool handlersInstalled = false;
  bool sendScheduled = false;
};

MSWindowsFileTransferService::MSWindowsFileTransferService(MSWindowsFileTransferServiceOptions options)
    : m_impl(std::make_unique<Impl>(std::move(options)))
{
  if (m_impl->events == nullptr || m_impl->eventTarget == nullptr) {
    return;
  }

  m_impl->events->addHandler(EventTypes::FileTransferControlReceived, m_impl->eventTarget, [this](const Event &event) {
    const auto *data = dynamic_cast<const FileTransferControlEventData *>(event.getDataObject());
    if (data != nullptr && !handleControl(data->message())) {
      LOG_WARN("rejected file-transfer control message");
    }
  });
  m_impl->events->addHandler(EventTypes::FileTransferDataReceived, m_impl->eventTarget, [this](const Event &event) {
    const auto *data = dynamic_cast<const FileTransferDataEventData *>(event.getDataObject());
    if (data != nullptr && !handleData(data->message)) {
      LOG_WARN("rejected file-transfer data message");
    }
  });
  m_impl->events->addHandler(EventTypes::FileTransferSendNextChunk, this, [this](const Event &) {
    m_impl->sendScheduled = false;
    (void)processNextOutgoingChunk();
  });
  m_impl->handlersInstalled = true;
}

MSWindowsFileTransferService::~MSWindowsFileTransferService()
{
  if (!m_impl->handlersInstalled) {
    return;
  }
  m_impl->events->removeHandler(EventTypes::FileTransferControlReceived, m_impl->eventTarget);
  m_impl->events->removeHandler(EventTypes::FileTransferDataReceived, m_impl->eventTarget);
  m_impl->events->removeHandler(EventTypes::FileTransferSendNextChunk, this);
}

bool MSWindowsFileTransferService::offerLocalFiles(
    std::string targetScreen, std::vector<FileTransferSourceCandidate> candidates
)
{
  if (m_impl->sessions.size() != 0 || !m_impl->sendControl || !m_impl->sendData) {
    return false;
  }

  const auto id = m_impl->createTransferId();
  const auto manifest =
      buildFileTransferSourceManifest(id, m_impl->localScreen, std::move(targetScreen), std::move(candidates));
  if (!manifest.ok()) {
    return false;
  }

  const auto offer = manifest.manifest.offer;
  if (!m_impl->sessions.registerOutgoing(std::move(manifest.manifest)).ok()) {
    return false;
  }
  if (!m_impl->sendControl(FileTransferControlMessage{offer})) {
    return failSession(id, FileTransferResultCode::NetworkUnavailable, false);
  }
  LOG_INFO("offered %zu file(s) to %s", offer.items.size(), offer.targetScreen.c_str());
  return true;
}

bool MSWindowsFileTransferService::handleControl(const FileTransferControlMessage &message)
{
  return std::visit(
      [this](const auto &value) -> bool {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferOffer>) {
          return handleOffer(value);
        } else if constexpr (std::is_same_v<Value, FileTransferDecision>) {
          return handleDecision(value);
        } else if constexpr (std::is_same_v<Value, FileTransferCancel>) {
          return handleCancel(value);
        } else {
          return handleResult(value);
        }
      },
      message
  );
}

bool MSWindowsFileTransferService::handleOffer(const FileTransferOffer &offer)
{
  if (!m_impl->sendControl || offer.targetScreen != m_impl->localScreen) {
    return false;
  }
  if (!m_impl->sessions.registerIncoming(offer).ok()) {
    return false;
  }

  if (!m_impl->receiveEnabled) {
    const auto decision = m_impl->sessions.decideIncoming(offer.id, FileTransferDecisionValue::Reject);
    const auto sent = decision.ok() && m_impl->sendControl(FileTransferControlMessage{*decision.decision});
    (void)m_impl->sessions.eraseTerminal(offer.id);
    return sent;
  }

  if (m_impl->destinationDirectory.empty() || !m_impl->destinationDirectory.is_absolute()) {
    const auto decision = m_impl->sessions.decideIncoming(offer.id, FileTransferDecisionValue::Reject);
    const auto sent = decision.ok() && m_impl->sendControl(FileTransferControlMessage{*decision.decision});
    (void)m_impl->sessions.eraseTerminal(offer.id);
    return sent;
  }

  std::error_code directoryError;
  std::filesystem::create_directories(m_impl->destinationDirectory, directoryError);
  auto writer = directoryError
                    ? MSWindowsFileTransferWriterCreateResult{.error = MSWindowsFileTransferIoError::DestinationDenied}
                    : MSWindowsFileTransferWriter::create(m_impl->destinationDirectory, offer);
  if (!writer.ok()) {
    const auto decision = m_impl->sessions.decideIncoming(offer.id, FileTransferDecisionValue::Reject);
    const auto sent = decision.ok() && m_impl->sendControl(FileTransferControlMessage{*decision.decision});
    (void)m_impl->sessions.eraseTerminal(offer.id);
    return sent;
  }

  const auto decision = m_impl->sessions.decideIncoming(offer.id, FileTransferDecisionValue::Accept);
  if (!decision.ok()) {
    return false;
  }
  m_impl->incoming.emplace(offer.id, std::move(writer.writer));
  if (!m_impl->sendControl(FileTransferControlMessage{*decision.decision})) {
    return failSession(offer.id, FileTransferResultCode::NetworkUnavailable, false);
  }
  LOG_INFO("accepted %zu file(s) from %s", offer.items.size(), offer.sourceScreen.c_str());
  return true;
}

bool MSWindowsFileTransferService::handleDecision(const FileTransferDecision &decision)
{
  if (!m_impl->sendControl || !m_impl->sendData || !m_impl->sessions.applyDecision(decision).ok()) {
    return false;
  }
  if (decision.decision == FileTransferDecisionValue::Reject) {
    LOG_INFO("file-transfer offer was rejected by %s", decision.targetScreen.c_str());
    (void)m_impl->sessions.eraseTerminal(decision.id);
    return true;
  }

  const auto *session = m_impl->sessions.find(decision.id);
  if (session == nullptr || !session->sourceManifest.has_value() || session->sourceManifest->sources.empty()) {
    return failSession(decision.id, FileTransferResultCode::ProtocolError, true);
  }
  auto opened = MSWindowsFileTransferReader::open(session->sourceManifest->sources.front());
  if (!opened.ok()) {
    return failSession(decision.id, sourceResultCodeFor(opened.error), true);
  }
  if (!m_impl->sessions.beginTransfer(decision.id).ok()) {
    return failSession(decision.id, FileTransferResultCode::ProtocolError, true);
  }

  m_impl->outgoing = Impl::Outgoing{.id = decision.id, .itemIndex = 0, .reader = std::move(opened.reader)};
  const FileTransferDataRoute route{
      .id = decision.id,
      .sourceScreen = session->offer.sourceScreen,
      .targetScreen = session->offer.targetScreen,
  };
  if (!m_impl->sendData(FileTransferDataMessage{FileTransferDataBegin{.route = route}})) {
    return failSession(decision.id, FileTransferResultCode::NetworkUnavailable, true);
  }
  scheduleNextOutgoingChunk();
  return true;
}

bool MSWindowsFileTransferService::handleCancel(const FileTransferCancel &cancel)
{
  if (!m_impl->sessions.applyCancel(cancel).ok()) {
    return false;
  }
  if (m_impl->outgoing.has_value() && m_impl->outgoing->id == cancel.id) {
    m_impl->outgoing.reset();
  }
  m_impl->incoming.erase(cancel.id);
  (void)m_impl->sessions.eraseTerminal(cancel.id);
  return true;
}

bool MSWindowsFileTransferService::handleResult(const FileTransferResult &result)
{
  if (!m_impl->sessions.applyResult(result).ok()) {
    return false;
  }
  if (m_impl->outgoing.has_value() && m_impl->outgoing->id == result.id) {
    m_impl->outgoing.reset();
  }
  m_impl->incoming.erase(result.id);
  (void)m_impl->sessions.eraseTerminal(result.id);
  return true;
}

bool MSWindowsFileTransferService::handleData(const FileTransferDataMessage &message)
{
  const auto &route = routeOf(message);
  const auto *session = m_impl->sessions.find(route.id);
  if (session == nullptr || session->role != FileTransferSessionRole::Receiver ||
      session->offer.sourceScreen != route.sourceScreen || session->offer.targetScreen != route.targetScreen) {
    return false;
  }
  const auto found = m_impl->incoming.find(route.id);
  if (found == m_impl->incoming.end()) {
    return false;
  }

  MSWindowsFileTransferWriterMutation mutation;
  const auto handled = std::visit(
      [&](const auto &value) -> bool {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferDataBegin>) {
          if (!m_impl->sessions.beginTransfer(route.id).ok()) {
            return false;
          }
          mutation = found->second->begin(value);
        } else if constexpr (std::is_same_v<Value, FileTransferDataChunk>) {
          mutation = found->second->writeChunk(value);
        } else if constexpr (std::is_same_v<Value, FileTransferDataItemEnd>) {
          mutation = found->second->endItem(value);
        } else {
          mutation = found->second->finish(value);
        }
        return mutation.ok();
      },
      message
  );
  if (!handled) {
    return failSession(route.id, destinationResultCodeFor(mutation.error), true);
  }

  if (!std::holds_alternative<FileTransferDataFinish>(message)) {
    return true;
  }

  const FileTransferResult result{
      .id = route.id,
      .sourceScreen = route.sourceScreen,
      .targetScreen = route.targetScreen,
      .status = FileTransferResultStatus::Completed,
      .code = FileTransferResultCode::None,
  };
  const auto sent = m_impl->sendControl && m_impl->sendControl(FileTransferControlMessage{result});
  const auto applied = m_impl->sessions.applyResult(result).ok();
  m_impl->incoming.erase(route.id);
  (void)m_impl->sessions.eraseTerminal(route.id);
  if (applied) {
    LOG_INFO("received and published %zu file(s)", mutation.publishedPaths.size());
  }
  return sent && applied;
}

bool MSWindowsFileTransferService::processNextOutgoingChunk()
{
  if (!m_impl->outgoing.has_value()) {
    return false;
  }
  auto &outgoing = *m_impl->outgoing;
  const auto *session = m_impl->sessions.find(outgoing.id);
  if (session == nullptr || !session->sourceManifest.has_value()) {
    return failSession(outgoing.id, FileTransferResultCode::ProtocolError, true);
  }
  const auto &manifest = *session->sourceManifest;
  if (outgoing.itemIndex >= manifest.sources.size()) {
    return failSession(outgoing.id, FileTransferResultCode::ProtocolError, true);
  }

  const FileTransferDataRoute route{
      .id = outgoing.id,
      .sourceScreen = session->offer.sourceScreen,
      .targetScreen = session->offer.targetScreen,
  };
  auto read = outgoing.reader->readNext();
  if (!read.ok()) {
    return failSession(outgoing.id, sourceResultCodeFor(read.error), true);
  }
  if (!read.bytes.empty() && !m_impl->sendData(
                                 FileTransferDataMessage{FileTransferDataChunk{
                                     .route = route,
                                     .itemIndex = outgoing.itemIndex,
                                     .offset = read.offset,
                                     .bytes = std::move(read.bytes),
                                 }}
                             )) {
    return failSession(outgoing.id, FileTransferResultCode::NetworkUnavailable, true);
  }

  if (!read.complete) {
    scheduleNextOutgoingChunk();
    return true;
  }
  if (!m_impl->sendData(
          FileTransferDataMessage{FileTransferDataItemEnd{
              .route = route,
              .itemIndex = outgoing.itemIndex,
              .size = manifest.sources[outgoing.itemIndex].size,
              .sha256 = read.sha256,
          }}
      )) {
    return failSession(outgoing.id, FileTransferResultCode::NetworkUnavailable, true);
  }

  ++outgoing.itemIndex;
  if (outgoing.itemIndex < manifest.sources.size()) {
    auto opened = MSWindowsFileTransferReader::open(manifest.sources[outgoing.itemIndex]);
    if (!opened.ok()) {
      return failSession(outgoing.id, sourceResultCodeFor(opened.error), true);
    }
    outgoing.reader = std::move(opened.reader);
    scheduleNextOutgoingChunk();
    return true;
  }

  std::uint64_t totalBytes = 0;
  for (const auto &source : manifest.sources) {
    totalBytes += source.size;
  }

  if (!m_impl->sendData(
          FileTransferDataMessage{FileTransferDataFinish{
              .route = route,
              .itemCount = static_cast<std::uint32_t>(manifest.sources.size()),
              .totalBytes = totalBytes,
          }}
      )) {
    return failSession(outgoing.id, FileTransferResultCode::NetworkUnavailable, true);
  }
  m_impl->outgoing.reset();
  return true;
}

std::size_t MSWindowsFileTransferService::activeSessionCount() const noexcept
{
  return m_impl->sessions.size();
}

void MSWindowsFileTransferService::scheduleNextOutgoingChunk()
{
  if (m_impl->events == nullptr || m_impl->sendScheduled || !m_impl->outgoing.has_value()) {
    return;
  }
  m_impl->sendScheduled = true;
  m_impl->events->addEvent(Event(EventTypes::FileTransferSendNextChunk, this));
}

bool MSWindowsFileTransferService::failSession(const TransferId &id, FileTransferResultCode code, bool notifyPeer)
{
  const auto *session = m_impl->sessions.find(id);
  if (session == nullptr) {
    return false;
  }
  const FileTransferResult result{
      .id = id,
      .sourceScreen = session->offer.sourceScreen,
      .targetScreen = session->offer.targetScreen,
      .status = FileTransferResultStatus::Failed,
      .code = code,
  };
  if (notifyPeer && m_impl->sendControl) {
    (void)m_impl->sendControl(FileTransferControlMessage{result});
  }
  (void)m_impl->sessions.applyResult(result);
  if (m_impl->outgoing.has_value() && m_impl->outgoing->id == id) {
    m_impl->outgoing.reset();
  }
  m_impl->incoming.erase(id);
  (void)m_impl->sessions.eraseTerminal(id);
  return false;
}

} // namespace deskflow::filetransfer
