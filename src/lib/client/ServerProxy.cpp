/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "client/Client.h"
#include "common/Settings.h"
#include "deskflow/CanonicalScancode.h"
#include "deskflow/Clipboard.h"
#include "deskflow/ClipboardChunk.h"
#include "deskflow/DeskflowException.h"
#include "deskflow/OptionTypes.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "deskflow/StreamChunker.h"
#include "deskflow/ipc/CoreIpc.h"
#include "io/IStream.h"

#include <bit>
#include <chrono>
#include <cstring>
#include <vector>

namespace {
// Keep a network burst from monopolizing the core event loop. Mouse motion is
// still coalesced within each slice, so a delayed packet cannot collapse an
// arbitrarily long movement into one visible cursor jump.
constexpr uint32_t kMaxMessagesPerDispatch = 32;
} // namespace

//
// ServerProxy
//

ServerProxy::ServerProxy(Client *client, deskflow::IStream *stream, IEventQueue *events)
    : ServerProxy(client, stream, events, client != nullptr ? client->getEventTarget() : nullptr)
{
}

ServerProxy::ServerProxy(Client *client, deskflow::IStream *stream, IEventQueue *events, void *clientEventTarget)
    : m_client(client),
      m_stream(stream),
      m_events(events),
      m_clientEventTarget(clientEventTarget)
{
  assert(m_client != nullptr);
  assert(m_stream != nullptr);

  // initialize modifier translation table
  for (KeyModifierID id = 0; id < kKeyModifierIDLast; ++id)
    m_modifierTranslationTable[id] = id;

  // handle data on stream
  m_events->addHandler(EventTypes::StreamInputReady, m_stream->getEventTarget(), [this](const auto &) {
    handleData();
  });
  m_events->addHandler(EventTypes::ClipboardSending, this, [this](const auto &e) {
    ClipboardChunk::send(m_stream, e.getDataObject());
  });
  m_events->addHandler(EventTypes::InputLanguageChanged, m_clientEventTarget, [this](const auto &e) {
    handleInputLanguageChanged(e);
  });

  // send heartbeat
  setKeepAliveRate(kKeepAliveRate);
}

ServerProxy::~ServerProxy()
{
  // the screen outlives this proxy, so a key held when the link dropped can
  // still be released
  releaseRawScancodeButtons();
  setKeepAliveRate(-1.0);
  m_events->removeHandler(EventTypes::StreamInputReady, m_stream->getEventTarget());
  m_events->removeHandler(EventTypes::ClipboardSending, this);
  m_events->removeHandler(EventTypes::InputLanguageChanged, m_clientEventTarget);
}

void ServerProxy::resetKeepAliveAlarm()
{
  if (m_keepAliveAlarmTimer != nullptr) {
    m_events->removeHandler(EventTypes::Timer, m_keepAliveAlarmTimer);
    m_events->deleteTimer(m_keepAliveAlarmTimer);
    m_keepAliveAlarmTimer = nullptr;
  }
  if (m_keepAliveAlarm > 0.0) {
    m_keepAliveAlarmTimer = m_events->newOneShotTimer(m_keepAliveAlarm, nullptr);
    m_events->addHandler(EventTypes::Timer, m_keepAliveAlarmTimer, [this](const auto &) { handleKeepAliveAlarm(); });
  }
}

void ServerProxy::setKeepAliveRate(double rate)
{
  m_keepAliveAlarm = rate * kKeepAlivesUntilDeath;
  resetKeepAliveAlarm();
}

void ServerProxy::handleData()
{
  // Handle a bounded slice. The stream only raises its ready event when the
  // input buffer transitions from empty, so queue another turn when data
  // remains after this slice.
  uint8_t code[4];
  uint32_t n = m_stream->read(code, 4);
  uint32_t messageCount = 0;
  while (n != 0 && messageCount < kMaxMessagesPerDispatch) {
    // verify we got an entire code
    if (n != 4) {
      LOG_ERR("incomplete message from server: %d bytes", n);
      requestDisconnect("incomplete message from server");
      return;
    }

    // parse message
    LOG_VERBOSE("msg from server: %c%c%c%c", code[0], code[1], code[2], code[3]);
    try {
      switch ((this->*m_parser)(code)) {
        using enum ConnectionResult;
      case Okay:
        break;

      case Unknown:
        LOG_ERR("invalid message from server: %c%c%c%c", code[0], code[1], code[2], code[3]);
        // not possible to determine message boundaries
        // read the whole stream to discard unkonwn data
        while (m_stream->read(nullptr, 4))
          ;
        break;

      case Disconnect:
        return;
      }
    } catch (const BadClientException &e) {
      LOG_ERR("protocol error from server: %s", e.what());
      ProtocolUtil::writef(m_stream, kMsgEBad);
      requestDisconnect("invalid message from server");
      return;
    }

    ++messageCount;

    // next message
    if (messageCount < kMaxMessagesPerDispatch) {
      n = m_stream->read(code, 4);
    }
  }

  flushCompressedMouse();

  if (m_stream->isReady()) {
    m_events->addEvent(Event(EventTypes::StreamInputReady, m_stream->getEventTarget()));
  }
}

ServerProxy::ConnectionResult ServerProxy::parseHandshakeMessage(const uint8_t *code)
{
  using enum ConnectionResult;
  using enum deskflow::core::ConnectionRefusal;

  if (memcmp(code, kMsgQInfo, 4) == 0) {
    queryInfo();
  }

  else if (memcmp(code, kMsgCInfoAck, 4) == 0) {
    infoAcknowledgment();
  }

  else if (memcmp(code, kMsgDSetOptions, 4) == 0) {
    setOptions();

    // handshake is complete
    m_parser = &ServerProxy::parseMessage;

    if (const auto missedKeyboardLayouts = m_layoutManager.getMissedLayouts(); !missedKeyboardLayouts.empty()) {
      LOG_WARN("server layouts missing on this computer: %s", missedKeyboardLayouts.c_str());
      ipcSendToClient("missingKeyboardLayouts", QString::fromStdString(missedKeyboardLayouts));
    }

    m_client->handshakeComplete();
  }

  else if (memcmp(code, kMsgCResetOptions, 4) == 0) {
    resetOptions();
  }

  else if (memcmp(code, kMsgCKeepAlive, 4) == 0) {
    // echo keep alives and reset alarm
    ProtocolUtil::writef(m_stream, kMsgCKeepAlive);
    resetKeepAliveAlarm();
  }

  else if (memcmp(code, kMsgCNoop, 4) == 0) {
    // accept and discard no-op
  }

  else if (memcmp(code, kMsgCClose, 4) == 0) {
    // server wants us to hangup
    LOG_VERBOSE("recv close");
    requestDisconnect(nullptr);
    return Disconnect;
  }

  else if (memcmp(code, kMsgEIncompatible, 4) == 0) {
    int32_t major;
    int32_t minor;
    ProtocolUtil::readf(m_stream, kMsgEIncompatible + 4, &major, &minor);
    LOG_ERR("server has incompatible version %d.%d", major, minor);
    requestRefuseConnection(IncompatibleVersion, "server has incompatible version");
    return Disconnect;
  }

  else if (memcmp(code, kMsgEBusy, 4) == 0) {
    LOG_ERR("server already has a connected client with name \"%s\"", m_client->getName().c_str());
    requestRefuseConnection(AlreadyConnected, "server already has a connected client with our name");
    return Disconnect;
  }

  else if (memcmp(code, kMsgEUnknown, 4) == 0) {
    LOG_ERR("server refused client with name \"%s\"", m_client->getName().c_str());
    requestRefuseConnection(UnknownClient, "server refused client with our name");
    return Disconnect;
  }

  else if (memcmp(code, kMsgEBad, 4) == 0) {
    LOG_ERR("server disconnected due to a protocol error");
    requestRefuseConnection(ProtocolError, "server reported a protocol error");
    return Disconnect;
  } else if (memcmp(code, kMsgDLanguageSynchronisation, 4) == 0) {
    setServerLanguages();
  } else {
    return Unknown;
  }

  return Okay;
}

ServerProxy::ConnectionResult ServerProxy::parseMessage(const uint8_t *code)
{
  using enum ConnectionResult;

  if (memcmp(code, kMsgDMouseMove, 4) == 0) {
    mouseMove();
  }

  else if (memcmp(code, kMsgDMouseRelMove, 4) == 0) {
    mouseRelativeMove();
  }

  else if (memcmp(code, kMsgDMouseWheel, 4) == 0) {
    mouseWheel();
  }

  else if (memcmp(code, kMsgDKeyDown, 4) == 0) {
    uint16_t id = 0;
    uint16_t mask = 0;
    uint16_t button = 0;
    ProtocolUtil::readf(m_stream, kMsgDKeyDown + 4, &id, &mask, &button);
    LOG_VERBOSE("recv key down id=0x%08x, mask=0x%04x, button=0x%04x", id, mask, button);

    keyDown(id, mask, button, "");
  }

  else if (memcmp(code, kMsgDInputLangControl, 4) == 0) {
    inputLanguageControl();
  }

  else if (memcmp(code, kMsgDKeyDownLang, 4) == 0) {
    std::string lang;
    uint16_t id = 0;
    uint16_t mask = 0;
    uint16_t button = 0;

    ProtocolUtil::readf(m_stream, kMsgDKeyDownLang + 4, &id, &mask, &button, &lang);
    LOG_VERBOSE("recv key down id=0x%08x, mask=0x%04x, button=0x%04x, lang=\"%s\"", id, mask, button, lang.c_str());

    keyDown(id, mask, button, lang);
  }

  else if (memcmp(code, kMsgDKeyUp, 4) == 0) {
    keyUp();
  }

  else if (memcmp(code, kMsgDMouseDown, 4) == 0) {
    mouseDown();
  }

  else if (memcmp(code, kMsgDMouseUp, 4) == 0) {
    mouseUp();
  }

  else if (memcmp(code, kMsgDKeyRepeat, 4) == 0) {
    keyRepeat();
  }

  else if (memcmp(code, kMsgCKeepAlive, 4) == 0) {
    // echo keep alives and reset alarm
    ProtocolUtil::writef(m_stream, kMsgCKeepAlive);
    resetKeepAliveAlarm();
  }

  else if (memcmp(code, kMsgCNoop, 4) == 0) {
    // accept and discard no-op
  }

  else if (memcmp(code, kMsgCEnter, 4) == 0) {
    enter();
  }

  else if (memcmp(code, kMsgCLeave, 4) == 0) {
    leave();
  }

  else if (memcmp(code, kMsgCClipboard, 4) == 0) {
    grabClipboard();
  }

  else if (memcmp(code, kMsgCScreenSaver, 4) == 0) {
    screensaver();
  }

  else if (memcmp(code, kMsgQInfo, 4) == 0) {
    queryInfo();
  }

  else if (memcmp(code, kMsgCInfoAck, 4) == 0) {
    infoAcknowledgment();
  }

  else if (memcmp(code, kMsgDClipboard, 4) == 0) {
    setClipboard();
  }

  else if (memcmp(code, kMsgCResetOptions, 4) == 0) {
    resetOptions();
  }

  else if (memcmp(code, kMsgDSetOptions, 4) == 0) {
    setOptions();
  }

  else if (memcmp(code, kMsgDSecureInputNotification, 4) == 0) {
    secureInputNotification();
  }

  else if (memcmp(code, kMsgCClose, 4) == 0) {
    // server wants us to hangup
    LOG_VERBOSE("recv close");
    requestDisconnect(nullptr);
    return Disconnect;
  } else if (memcmp(code, kMsgEBad, 4) == 0) {
    LOG_ERR("server disconnected due to a protocol error");
    requestDisconnect("server reported a protocol error");
    return Disconnect;
  } else {
    return Unknown;
  }

  // send a reply.  this is intended to work around a delay when
  // running a linux server and an OS X (any BSD?) client.  the
  // client waits to send an ACK (if the system control flag
  // net.inet.tcp.delayed_ack is 1) in hopes of piggybacking it
  // on a data packet.  we provide that packet here.  i don't
  // know why a delayed ACK should cause the server to wait since
  // TCP_NODELAY is enabled.
  ProtocolUtil::writef(m_stream, kMsgCNoop);

  return Okay;
}

void ServerProxy::handleKeepAliveAlarm()
{
  LOG_INFO("server is dead");
  requestDisconnect("server is not responding");
}

void ServerProxy::requestDisconnect(const char *message)
{
  m_events->addEvent(Event(
      EventTypes::ClientDisconnectRequested, m_stream->getEventTarget(),
      new Client::DisconnectRequest(Client::DisconnectRequest::Kind::Disconnect, message)
  ));
}

void ServerProxy::requestRefuseConnection(deskflow::core::ConnectionRefusal reason, const char *message)
{
  m_events->addEvent(Event(
      EventTypes::ClientDisconnectRequested, m_stream->getEventTarget(), new Client::DisconnectRequest(reason, message)
  ));
}

void ServerProxy::onInfoChanged()
{
  // ignore mouse motion until we receive acknowledgment of our info
  // change message.
  m_ignoreMouse = true;

  // send info update
  queryInfo();
}

bool ServerProxy::onGrabClipboard(ClipboardID id)
{
  LOG_VERBOSE("sending clipboard %d changed", id);
  ProtocolUtil::writef(m_stream, kMsgCClipboard, id, m_seqNum);
  return true;
}

void ServerProxy::onClipboardChanged(ClipboardID id, const IClipboard *clipboard)
{
  std::string data = IClipboard::marshall(clipboard);
  LOG_DEBUG("sending clipboard %d seqnum=%d", id, m_seqNum);

  StreamChunker::sendClipboard(data, data.size(), id, m_seqNum, m_events, this);
}

void ServerProxy::flushCompressedMouse()
{
  if (m_compressMouse) {
    m_compressMouse = false;
    m_client->mouseMove(m_xMouse, m_yMouse);
  }
  if (m_compressMouseRelative) {
    m_compressMouseRelative = false;
    m_client->mouseRelativeMove(m_dxMouse, m_dyMouse);
    m_dxMouse = 0;
    m_dyMouse = 0;
  }
}

void ServerProxy::sendInfo(const ClientInfo &info)
{
  LOG_VERBOSE("sending info shape=%d,%d %dx%d", info.m_x, info.m_y, info.m_w, info.m_h);
  ProtocolUtil::writef(m_stream, kMsgDInfo, info.m_x, info.m_y, info.m_w, info.m_h, 0, info.m_mx, info.m_my);
}

void ServerProxy::sendDisplayLayout()
{
  if (m_client->protocolMinorVersion() < kProtocolDisplayLayoutMinorVersion) {
    return;
  }

  const auto layout = m_client->getDisplayLayout();
  std::vector<uint32_t> packed;
  packed.reserve(layout.size() * 4);
  for (const auto &display : layout) {
    if (display.width <= 0 || display.height <= 0) {
      continue;
    }
    packed.push_back(std::bit_cast<uint32_t>(display.x));
    packed.push_back(std::bit_cast<uint32_t>(display.y));
    packed.push_back(std::bit_cast<uint32_t>(display.width));
    packed.push_back(std::bit_cast<uint32_t>(display.height));
  }

  if (!packed.empty()) {
    ProtocolUtil::writef(m_stream, kMsgCDisplayLayout, &packed);
  }
}

void ServerProxy::maybeSendForegroundFullscreen(bool force)
{
  if (m_client->protocolMinorVersion() < kProtocolDisplayLayoutMinorVersion) {
    return;
  }

  using namespace std::chrono_literals;
  const auto now = std::chrono::steady_clock::now();
  if (!force && m_lastFullscreenCheck.time_since_epoch().count() != 0 && now - m_lastFullscreenCheck < 100ms) {
    return;
  }
  m_lastFullscreenCheck = now;

  const bool fullscreen = m_client->isForegroundFullscreen();
  if (force || !m_lastFullscreenState || *m_lastFullscreenState != fullscreen) {
    ProtocolUtil::writef(m_stream, kMsgCForegroundFullscreen, static_cast<int8_t>(fullscreen ? 1 : 0));
    m_lastFullscreenState = fullscreen;
  }
}

KeyID ServerProxy::translateKey(KeyID id) const
{
  static const KeyID s_translationTable[kKeyModifierIDLast][2] = {
      {kKeyNone, kKeyNone},     {kKeyShift_L, kKeyShift_R}, {kKeyControl_L, kKeyControl_R}, {kKeyAlt_L, kKeyAlt_R},
      {kKeyMeta_L, kKeyMeta_R}, {kKeySuper_L, kKeySuper_R}, {kKeyAltGr, kKeyAltGr}
  };

  KeyModifierID id2 = kKeyModifierIDNull;
  uint32_t side = 0;
  switch (id) {
  case kKeyShift_L:
    id2 = kKeyModifierIDShift;
    side = 0;
    break;

  case kKeyShift_R:
    id2 = kKeyModifierIDShift;
    side = 1;
    break;

  case kKeyControl_L:
    id2 = kKeyModifierIDControl;
    side = 0;
    break;

  case kKeyControl_R:
    id2 = kKeyModifierIDControl;
    side = 1;
    break;

  case kKeyAlt_L:
    id2 = kKeyModifierIDAlt;
    side = 0;
    break;

  case kKeyAlt_R:
    id2 = kKeyModifierIDAlt;
    side = 1;
    break;

  case kKeyAltGr:
    id2 = kKeyModifierIDAltGr;
    side = 1; // there is only one alt gr key on the right side
    break;

  case kKeyMeta_L:
    id2 = kKeyModifierIDMeta;
    side = 0;
    break;

  case kKeyMeta_R:
    id2 = kKeyModifierIDMeta;
    side = 1;
    break;

  case kKeySuper_L:
    id2 = kKeyModifierIDSuper;
    side = 0;
    break;

  case kKeySuper_R:
    id2 = kKeyModifierIDSuper;
    side = 1;
    break;

  default:
    break;
  }

  if (id2 != kKeyModifierIDNull) {
    return s_translationTable[m_modifierTranslationTable[id2]][side];
  } else {
    return id;
  }
}

KeyModifierMask ServerProxy::translateModifierMask(KeyModifierMask mask) const
{
  static const KeyModifierMask s_masks[kKeyModifierIDLast] = {0x0000,          KeyModifierShift, KeyModifierControl,
                                                              KeyModifierAlt,  KeyModifierMeta,  KeyModifierSuper,
                                                              KeyModifierAltGr};

  KeyModifierMask newMask = mask & ~(KeyModifierShift | KeyModifierControl | KeyModifierAlt | KeyModifierMeta |
                                     KeyModifierSuper | KeyModifierAltGr);
  if ((mask & KeyModifierShift) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDShift]];
  }
  if ((mask & KeyModifierControl) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDControl]];
  }
  if ((mask & KeyModifierAlt) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDAlt]];
  }
  if ((mask & KeyModifierAltGr) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDAltGr]];
  }
  if ((mask & KeyModifierMeta) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDMeta]];
  }
  if ((mask & KeyModifierSuper) != 0) {
    newMask |= s_masks[m_modifierTranslationTable[kKeyModifierIDSuper]];
  }
  return newMask;
}

void ServerProxy::enter()
{
  // parse
  int16_t x;
  int16_t y;
  uint16_t mask;
  uint32_t seqNum;
  ProtocolUtil::readf(m_stream, kMsgCEnter + 4, &x, &y, &seqNum, &mask);
  LOG_VERBOSE("recv enter, %d,%d %d %04x", x, y, seqNum, mask);

  // discard old compressed mouse motion, if any
  m_compressMouse = false;
  m_compressMouseRelative = false;
  m_dxMouse = 0;
  m_dyMouse = 0;
  m_seqNum = seqNum;
  m_serverLayout = "";
  m_isUserNotifiedAboutLayoutSyncError = false;

  // defensive: nothing should still be held from the previous visit
  releaseRawScancodeButtons();

  // forward
  m_client->enter(x, y, seqNum, static_cast<KeyModifierMask>(mask), false);
  maybeSendForegroundFullscreen(true);
}

void ServerProxy::leave()
{
  // parse
  LOG_VERBOSE("recv leave");

  // send last mouse motion
  flushCompressedMouse();

  // Raw keys bypass KeyState, so fakeAllKeysUp() in Client::leave() cannot see
  // them. Release them here or they stay down after the cursor has gone.
  releaseRawScancodeButtons();

  // forward
  m_client->leave();
}

void ServerProxy::setClipboard()
{
  // parse
  ClipboardID id;
  uint32_t seq;

  auto r = ClipboardChunk::assemble(
      m_stream, m_clipboardDataCached, id, seq, m_clipboardChunkState, m_client->getMaximumClipboardReceiveSizeBytes()
  );

  if (r == TransferState::Started) {
    size_t size = ClipboardChunk::getExpectedSize(m_clipboardChunkState);
    LOG_DEBUG("receiving clipboard %d size=%zu", id, size);
  } else if (r == TransferState::Finished) {
    LOG_DEBUG("received clipboard %d size=%zu", id, m_clipboardDataCached.size());

    // forward
    Clipboard clipboard;
    clipboard.unmarshall(m_clipboardDataCached, 0);
    m_client->setClipboard(id, &clipboard);
    m_clipboardDataCached.clear();
    m_clipboardDataCached.shrink_to_fit();

    LOG_INFO("clipboard was updated");
  } else if (r == TransferState::Error) {
    requestDisconnect("invalid clipboard data from server");
  }
}

void ServerProxy::grabClipboard()
{
  // parse
  ClipboardID id;
  uint32_t seqNum;
  ProtocolUtil::readf(m_stream, kMsgCClipboard + 4, &id, &seqNum);
  LOG_DEBUG("recv grab clipboard %d", id);

  // validate
  if (id >= kClipboardEnd) {
    return;
  }

  // forward
  m_client->grabClipboard(id);
}

void ServerProxy::keyDown(uint16_t id, uint16_t mask, uint16_t button, const std::string &lang)
{
  // get mouse up to date
  flushCompressedMouse();
  if (!m_inputLanguageStatus.isInputMethod()) {
    setActiveServerLanguage(lang);
  }

  // Only a button the server marked as a canonical Set-1 scancode may be
  // injected raw. Anything else is a platform key button of unknown meaning.
  const auto canonicalButton = deskflow::scancode::stripCanonical(button);
  if (deskflow::scancode::isCanonical(button) && deskflow::scancode::isSet1Scancode(canonicalButton) &&
      rawScancodeEnabled() && m_client->rawKeyDown(canonicalButton, static_cast<KeyModifierMask>(mask))) {
    m_rawScancodeButtons.insert(button);
    return;
  }

  // translate
  KeyID id2 = m_inputLanguageStatus.isInputMethod() ? static_cast<KeyID>(id) : translateKey(static_cast<KeyID>(id));
  KeyModifierMask mask2 = translateModifierMask(static_cast<KeyModifierMask>(mask));
  if (id2 != static_cast<KeyID>(id) || mask2 != static_cast<KeyModifierMask>(mask))
    LOG_VERBOSE("key down translated to id=0x%08x, mask=0x%04x", id2, mask2);

  // forward
  m_client->keyDown(id2, mask2, canonicalButton, lang);
}

void ServerProxy::keyRepeat()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  uint16_t id;
  uint16_t mask;
  uint16_t count;
  uint16_t button;
  std::string lang;
  ProtocolUtil::readf(m_stream, kMsgDKeyRepeat + 4, &id, &mask, &count, &button, &lang);
  LOG(
      (CLOG_VERBOSE "recv key repeat id=0x%08x, mask=0x%04x, count=%d, "
                    "button=0x%04x, lang=\"%s\"",
       id, mask, count, button, lang.c_str())
  );

  if (m_rawScancodeButtons.contains(button)) {
    m_client->rawKeyRepeat(deskflow::scancode::stripCanonical(button), static_cast<KeyModifierMask>(mask), count);
    return;
  }

  // translate
  KeyID id2 = m_inputLanguageStatus.isInputMethod() ? static_cast<KeyID>(id) : translateKey(static_cast<KeyID>(id));
  KeyModifierMask mask2 = translateModifierMask(static_cast<KeyModifierMask>(mask));
  if (id2 != static_cast<KeyID>(id) || mask2 != static_cast<KeyModifierMask>(mask))
    LOG_VERBOSE("key repeat translated to id=0x%08x, mask=0x%04x", id2, mask2);

  // forward
  m_client->keyRepeat(id2, mask2, count, deskflow::scancode::stripCanonical(button), lang);
}

void ServerProxy::keyUp()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  uint16_t id;
  uint16_t mask;
  uint16_t button;
  ProtocolUtil::readf(m_stream, kMsgDKeyUp + 4, &id, &mask, &button);
  LOG_VERBOSE("recv key up id=0x%08x, mask=0x%04x, button=0x%04x", id, mask, button);

  if (m_rawScancodeButtons.erase(button) != 0) {
    m_client->rawKeyUp(deskflow::scancode::stripCanonical(button), static_cast<KeyModifierMask>(mask));
    return;
  }

  // translate
  KeyID id2 = m_inputLanguageStatus.isInputMethod() ? static_cast<KeyID>(id) : translateKey(static_cast<KeyID>(id));
  KeyModifierMask mask2 = translateModifierMask(static_cast<KeyModifierMask>(mask));
  if (id2 != static_cast<KeyID>(id) || mask2 != static_cast<KeyModifierMask>(mask))
    LOG_VERBOSE("key up translated to id=0x%08x, mask=0x%04x", id2, mask2);

  // forward
  m_client->keyUp(id2, mask2, deskflow::scancode::stripCanonical(button));
}

void ServerProxy::mouseDown()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  int8_t id;
  ProtocolUtil::readf(m_stream, kMsgDMouseDown + 4, &id);
  LOG_VERBOSE("recv mouse down id=%d", id);

  // forward
  m_client->mouseDown(static_cast<ButtonID>(id));
}

void ServerProxy::mouseUp()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  int8_t id;
  ProtocolUtil::readf(m_stream, kMsgDMouseUp + 4, &id);
  LOG_VERBOSE("recv mouse up id=%d", id);

  // forward
  m_client->mouseUp(static_cast<ButtonID>(id));
}

void ServerProxy::mouseMove()
{
  // parse
  bool ignore;
  int16_t x;
  int16_t y;
  ProtocolUtil::readf(m_stream, kMsgDMouseMove + 4, &x, &y);

  // note if we should ignore the move
  ignore = m_ignoreMouse;

  // compress mouse motion events if more input follows
  if (!ignore && !m_compressMouse && m_stream->isReady()) {
    m_compressMouse = true;
  }

  // if compressing then ignore the motion but record it
  if (m_compressMouse) {
    m_compressMouseRelative = false;
    ignore = true;
    m_xMouse = x;
    m_yMouse = y;
    m_dxMouse = 0;
    m_dyMouse = 0;
  }
  LOG_VERBOSE("recv mouse move %d,%d", x, y);

  // forward
  if (!ignore) {
    m_client->mouseMove(x, y);
  }
  maybeSendForegroundFullscreen();
}

void ServerProxy::mouseRelativeMove()
{
  // parse
  bool ignore;
  int16_t dx;
  int16_t dy;
  ProtocolUtil::readf(m_stream, kMsgDMouseRelMove + 4, &dx, &dy);

  // note if we should ignore the move
  ignore = m_ignoreMouse;

  // compress mouse motion events if more input follows
  if (!ignore && !m_compressMouseRelative && m_stream->isReady()) {
    m_compressMouseRelative = true;
  }

  // if compressing then ignore the motion but record it
  if (m_compressMouseRelative) {
    ignore = true;
    m_dxMouse += dx;
    m_dyMouse += dy;
  }
  LOG_VERBOSE("recv mouse relative move %d,%d", dx, dy);

  // forward
  if (!ignore) {
    m_client->mouseRelativeMove(dx, dy);
  }
  maybeSendForegroundFullscreen();
}

void ServerProxy::mouseWheel()
{
  // get mouse up to date
  flushCompressedMouse();

  // parse
  int16_t xDelta;
  int16_t yDelta;
  ProtocolUtil::readf(m_stream, kMsgDMouseWheel + 4, &xDelta, &yDelta);
  LOG_VERBOSE("recv mouse wheel %+d,%+d", xDelta, yDelta);

  // forward
  m_client->mouseWheel(xDelta, yDelta);
}

void ServerProxy::screensaver()
{
  // parse
  int8_t on;
  ProtocolUtil::readf(m_stream, kMsgCScreenSaver + 4, &on);
  LOG_VERBOSE("recv screen saver on=%d", on);

  // forward
  m_client->screensaver(on != 0);
}

void ServerProxy::resetOptions()
{
  // parse
  LOG_VERBOSE("recv reset options");

  releaseRawScancodeButtons();

  // forward
  m_client->resetOptions();

  // reset keep alive
  setKeepAliveRate(kKeepAliveRate);

  // reset modifier translation table
  for (KeyModifierID id = 0; id < kKeyModifierIDLast; ++id) {
    m_modifierTranslationTable[id] = id;
  }
}

void ServerProxy::setOptions()
{
  // parse
  OptionsList options;
  ProtocolUtil::readf(m_stream, kMsgDSetOptions + 4, &options);
  LOG_VERBOSE("recv set options size=%d", options.size());

  if (options.size() % 2 != 0) {
    LOG_ERR("options are the incorrect size, can not process them");
    return;
  }

  // forward
  m_client->setOptions(options);

  // update modifier table
  for (uint32_t i = 0, n = (uint32_t)options.size(); i < n; i += 2) {
    KeyModifierID id = kKeyModifierIDNull;
    if (options[i] == kOptionModifierMapForShift) {
      id = kKeyModifierIDShift;
    } else if (options[i] == kOptionModifierMapForControl) {
      id = kKeyModifierIDControl;
    } else if (options[i] == kOptionModifierMapForAlt) {
      id = kKeyModifierIDAlt;
    } else if (options[i] == kOptionModifierMapForAltGr) {
      id = kKeyModifierIDAltGr;
    } else if (options[i] == kOptionModifierMapForMeta) {
      id = kKeyModifierIDMeta;
    } else if (options[i] == kOptionModifierMapForSuper) {
      id = kKeyModifierIDSuper;
    } else if (options[i] == kOptionHeartbeat) {
      // update keep alive
      setKeepAliveRate(1.0e-3 * static_cast<double>(options[i + 1]));
    }

    if (id != kKeyModifierIDNull) {
      m_modifierTranslationTable[id] = std::clamp<KeyModifierMask>(options[i + 1], 0, kKeyModifierIDLast - 1);
      LOG_VERBOSE("modifier %d mapped to %d", id, m_modifierTranslationTable[id]);
    }
  }
}

void ServerProxy::queryInfo()
{
  ClientInfo info;
  m_client->getShape(info.m_x, info.m_y, info.m_w, info.m_h);
  m_client->getCursorPos(info.m_mx, info.m_my);
  sendInfo(info);
  sendDisplayLayout();
  maybeSendForegroundFullscreen(true);
}

void ServerProxy::infoAcknowledgment()
{
  LOG_VERBOSE("recv info acknowledgment");
  m_ignoreMouse = false;
}

void ServerProxy::secureInputNotification()
{
  std::string app;
  ProtocolUtil::readf(m_stream, kMsgDSecureInputNotification + 4, &app);
  LOG_INFO("application \"%s\" is blocking the keyboard", app.c_str());
}

void ServerProxy::setServerLanguages()
{
  std::string serverLayout;
  ProtocolUtil::readf(m_stream, kMsgDLanguageSynchronisation + 4, &serverLayout);
  m_layoutManager.setRemoteLayouts(serverLayout);
}

void ServerProxy::inputLanguageControl()
{
  int8_t action = 0;
  std::string target;
  ProtocolUtil::readf(m_stream, kMsgDInputLangControl + 4, &action, &target);
  if (action < static_cast<int8_t>(deskflow::InputLanguageAction::Toggle) ||
      action > static_cast<int8_t>(deskflow::InputLanguageAction::Query)) {
    LOG_WARN("server sent invalid input language action %d", action);
    sendInputLanguageStatus(m_client->inputLanguageStatus());
    return;
  }

  if (Settings::value(Settings::Core::ImeSync).toBool()) {
    m_client->inputLanguageControl(static_cast<deskflow::InputLanguageAction>(action), target);
  }
  sendInputLanguageStatus(m_client->inputLanguageStatus());
}

void ServerProxy::handleInputLanguageChanged(const Event &event)
{
  const auto *status = dynamic_cast<const deskflow::InputLanguageStatus *>(event.getDataObject());
  if (status != nullptr) {
    sendInputLanguageStatus(*status);
  }
}

void ServerProxy::sendInputLanguageStatus(const deskflow::InputLanguageStatus &status)
{
  if (status.m_sourceId.empty() && status.m_category == deskflow::InputLanguageCategory::Unknown) {
    LOG_VERBOSE("input language status is unavailable on this platform");
  }

  m_inputLanguageStatus = status;
  const int8_t category = static_cast<int8_t>(status.m_category);
  const int8_t composing = status.m_composing ? 1 : 0;

  // A server below 1.9 cannot parse CILS. A server still waiting for DINF
  // accepts handshake messages only. In either case, an unexpected code costs
  // it the rest of the stream, so keep the status local to the GUI.
  const bool handshakeComplete = m_parser == &ServerProxy::parseMessage;
  if (m_client->protocolMinorVersion() >= 9 && handshakeComplete) {
    ProtocolUtil::writef(m_stream, kMsgCInputLangStatus, &status.m_sourceId, category, composing);
  } else {
    LOG_VERBOSE(
        "not reporting input language status: server protocol 1.%d, handshake complete=%d",
        m_client->protocolMinorVersion(), handshakeComplete
    );
  }

  ipcSendToClient(
      "inputLanguageStatus",
      QStringLiteral("local|%1|%2|%3").arg(QString::fromStdString(status.m_sourceId)).arg(category).arg(composing)
  );
}

void ServerProxy::releaseRawScancodeButtons()
{
  for (const auto button : m_rawScancodeButtons) {
    m_client->rawKeyUp(deskflow::scancode::stripCanonical(button), 0);
  }
  m_rawScancodeButtons.clear();
}

bool ServerProxy::rawScancodeEnabled() const
{
  const auto mode = Settings::value(Settings::Client::CjkRawScancode).toString().toLower();
  return mode == QStringLiteral("on") || (mode == QStringLiteral("auto") && m_inputLanguageStatus.isInputMethod());
}

void ServerProxy::setActiveServerLanguage(const std::string_view &language)
{
  if (!language.empty() && (language.size() > 0)) {
    if (m_serverLayout != language) {
      m_isUserNotifiedAboutLayoutSyncError = false;
      m_serverLayout = language;
    }

    if (!m_layoutManager.isLayoutInstalled(m_serverLayout)) {
      if (!m_isUserNotifiedAboutLayoutSyncError) {
        LOG_WARN("current server layout is not installed on client");
        m_isUserNotifiedAboutLayoutSyncError = true;
      }
    } else {
      m_isUserNotifiedAboutLayoutSyncError = false;
    }
  } else {
    LOG_VERBOSE("active server layout is empty");
  }
}
