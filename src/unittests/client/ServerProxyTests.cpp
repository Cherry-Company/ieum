/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerProxyTests.h"

#include "../shared/FakePlatformScreen.h"
#include "base/Event.h"
#include "base/EventQueueTimer.h"
#include "base/IEventQueue.h"
#include "client/Client.h"
#include "client/ServerProxy.h"
#include "deskflow/AppUtil.h"
#include "deskflow/DeskflowException.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/Screen.h"
#include "io/IStream.h"
#include "net/IDataSocket.h"
#include "net/ISocketFactory.h"
#include "net/NetworkAddress.h"
#include "server/ClientProxy1_0.h"
#include "server/ClientProxy1_11.h"
#include "server/Server.h"

#include <QScopeGuard>
#include <QTest>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

class TestAppUtil : public AppUtil
{
public:
  int run() override
  {
    return 0;
  }

  void startNode() override
  {
  }

  std::vector<std::string> getKeyboardLayoutList() override
  {
    return {"en"};
  }

  std::string getCurrentLanguageCode() override
  {
    return "en";
  }
};

class FakeStream : public deskflow::IStream
{
public:
  void push(const std::string &bytes)
  {
    m_chunks.push_back(bytes);
  }

  void failReadAfter(size_t successfulReads)
  {
    m_readsBeforeFailure = successfulReads;
  }

  void close() override
  {
    m_chunks.clear();
    m_inputShutdown = true;
    m_closed = true;
  }

  uint32_t read(void *buffer, uint32_t size) override
  {
    if (m_readsBeforeFailure.has_value()) {
      if (*m_readsBeforeFailure == 0) {
        m_readsBeforeFailure.reset();
        throw std::bad_alloc();
      }
      --*m_readsBeforeFailure;
    }

    if (m_inputShutdown || m_chunks.empty() || size == 0) {
      return 0;
    }

    auto &front = m_chunks.front();
    const size_t bytesToRead = std::min(static_cast<size_t>(size), front.size());
    if (buffer != nullptr) {
      std::memcpy(buffer, front.data(), bytesToRead);
    }

    front.erase(0, bytesToRead);
    if (front.empty()) {
      m_chunks.pop_front();
    }
    return static_cast<uint32_t>(bytesToRead);
  }

  void write(const void *buffer, uint32_t size) override
  {
    m_written.append(static_cast<const char *>(buffer), size);
  }

  void flush() override
  {
  }

  void shutdownInput() override
  {
    close();
  }

  void shutdownOutput() override
  {
  }

  void *getEventTarget() const override
  {
    return const_cast<FakeStream *>(this);
  }

  bool isReady() const override
  {
    return !m_inputShutdown && !m_chunks.empty();
  }

  uint32_t getSize() const override
  {
    size_t total = 0;
    for (const auto &chunk : m_chunks) {
      total += chunk.size();
    }
    return static_cast<uint32_t>(std::min<size_t>(total, UINT32_MAX));
  }

  bool closed() const
  {
    return m_closed;
  }

  std::string takeWritten()
  {
    return std::exchange(m_written, {});
  }

private:
  std::deque<std::string> m_chunks;
  std::optional<size_t> m_readsBeforeFailure;
  bool m_inputShutdown = false;
  bool m_closed = false;
  std::string m_written;
};

class RecordingEventQueue : public IEventQueue
{
public:
  ~RecordingEventQueue() override
  {
    clearAddedEvents();
  }

  void clearAddedEvents()
  {
    for (const auto &event : m_addedEvents) {
      Event::deleteData(event);
    }
    m_addedEvents.clear();
  }

  int loop() override
  {
    return 0;
  }

  void adoptBuffer(IEventQueueBuffer *) override
  {
  }

  bool getEvent(Event &, double = -1.0) override
  {
    return false;
  }

  bool dispatchEvent(const Event &event) override
  {
    auto handler = m_handlers.find(HandlerKey{event.getType(), event.getTarget()});
    if (handler == m_handlers.end()) {
      handler = m_handlers.find(HandlerKey{EventTypes::Unknown, event.getTarget()});
      if (handler == m_handlers.end()) {
        return false;
      }
    }

    const auto callback = handler->second;
    callback(event);
    return true;
  }

  void addEvent(Event &&event) override
  {
    m_addedEvents.emplace_back(std::move(event));
  }

  EventQueueTimer *newTimer(double duration, void *) override
  {
    m_repeatingTimer = makeTimer();
    m_repeatingDuration = duration;
    return m_repeatingTimer;
  }

  EventQueueTimer *newOneShotTimer(double, void *) override
  {
    ++m_oneShotTimerCreations;
    m_lastOneShotTimer = makeTimer();
    return m_lastOneShotTimer;
  }

  void deleteTimer(EventQueueTimer *timer) override
  {
    m_deletedTimers.insert(timer);
  }

  void addHandler(EventTypes type, void *target, const EventHandler &handler) override
  {
    m_handlers[HandlerKey{type, target}] = handler;
  }

  void removeHandler(EventTypes type, void *target) override
  {
    m_handlers.erase(HandlerKey{type, target});
  }

  void removeHandlers(void *target) override
  {
    for (auto it = m_handlers.begin(); it != m_handlers.end();) {
      if (it->first.target == target) {
        it = m_handlers.erase(it);
      } else {
        ++it;
      }
    }
  }

  void waitForReady() const override
  {
  }

  void *getSystemTarget() override
  {
    return this;
  }

  EventQueueTimer *timer()
  {
    return m_lastOneShotTimer;
  }

  EventQueueTimer *repeatingTimer() const
  {
    return m_repeatingTimer;
  }
  double repeatingDuration() const
  {
    return m_repeatingDuration;
  }
  bool timerDeleted(EventQueueTimer *timer) const
  {
    return m_deletedTimers.contains(timer);
  }

  const std::vector<Event> &addedEvents() const
  {
    return m_addedEvents;
  }

  bool dispatchNextAddedEvent()
  {
    if (m_addedEvents.empty()) {
      return false;
    }

    Event event = std::move(m_addedEvents.front());
    m_addedEvents.erase(m_addedEvents.begin());
    const bool handled = dispatchEvent(event);
    Event::deleteData(event);
    return handled;
  }

  size_t oneShotTimerCreations() const
  {
    return m_oneShotTimerCreations;
  }

private:
  struct HandlerKey
  {
    EventTypes type;
    void *target;

    bool operator<(const HandlerKey &other) const
    {
      if (type != other.type) {
        return static_cast<uint32_t>(type) < static_cast<uint32_t>(other.type);
      }
      return std::less<void *>{}(target, other.target);
    }
  };

  EventQueueTimer *makeTimer()
  {
    m_timers.push_back(std::make_unique<EventQueueTimer>());
    return m_timers.back().get();
  }

  std::vector<std::unique_ptr<EventQueueTimer>> m_timers;
  std::set<EventQueueTimer *> m_deletedTimers;
  EventQueueTimer *m_lastOneShotTimer = nullptr;
  EventQueueTimer *m_repeatingTimer = nullptr;
  double m_repeatingDuration = 0.0;
  size_t m_oneShotTimerCreations = 0;
  std::map<HandlerKey, EventHandler> m_handlers;
  std::vector<Event> m_addedEvents;
};

class FakeSocket : public IDataSocket
{
public:
  explicit FakeSocket(IEventQueue *events) : IDataSocket(events)
  {
  }
  FakeStream stream;

  void bind(const NetworkAddress &) override
  {
  }
  void connect(const NetworkAddress &) override
  {
  }
  bool isFatal() const override
  {
    return false;
  }
  void close() override
  {
    stream.close();
  }
  void *getEventTarget() const override
  {
    return const_cast<FakeSocket *>(this);
  }
  uint32_t read(void *buffer, uint32_t size) override
  {
    return stream.read(buffer, size);
  }
  void write(const void *buffer, uint32_t size) override
  {
    stream.write(buffer, size);
  }
  void flush() override
  {
  }
  void shutdownInput() override
  {
    stream.shutdownInput();
  }
  void shutdownOutput() override
  {
  }
  bool isReady() const override
  {
    return stream.isReady();
  }
  uint32_t getSize() const override
  {
    return stream.getSize();
  }

  void pushPacket(const std::string &payload)
  {
    const auto length = static_cast<uint32_t>(payload.size());
    std::string packet;
    for (const int shift : {24, 16, 8, 0}) {
      packet.push_back(static_cast<char>((length >> shift) & 0xff));
    }
    stream.push(packet + payload);
  }
};

class FakeSocketFactory : public ISocketFactory
{
public:
  explicit FakeSocketFactory(IEventQueue *events) : m_events(events)
  {
  }
  mutable FakeSocket *socket = nullptr;
  IDataSocket *create(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    socket = new FakeSocket(m_events);
    return socket;
  }
  IListenSocket *createListen(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    return nullptr;
  }

private:
  IEventQueue *m_events;
};

class TestServerProxy : public ServerProxy
{
public:
  TestServerProxy(Client *client, deskflow::IStream *stream, IEventQueue *events)
      : ServerProxy(client, stream, events, nullptr)
  {
  }

  bool parseHandshakeMessageReturnsDisconnect(const uint8_t *code)
  {
    return parseHandshakeMessage(code) == ConnectionResult::Disconnect;
  }

  bool parseHandshakeMessageReturnsOkay(const uint8_t *code)
  {
    return parseHandshakeMessage(code) == ConnectionResult::Okay;
  }

  void parseHandshake(const QByteArray &code)
  {
    (void)parseHandshakeMessage(reinterpret_cast<const uint8_t *>(code.constData()));
  }

  void parseNormal(const QByteArray &code)
  {
    (void)parseMessage(reinterpret_cast<const uint8_t *>(code.constData()));
  }
};

std::string codeOnly(const char *message)
{
  return {message, 4};
}

void appendU16(std::string &bytes, uint16_t value)
{
  bytes.push_back(static_cast<char>((value >> 8) & 0xff));
  bytes.push_back(static_cast<char>(value & 0xff));
}

std::string validInfoMessage()
{
  std::string message = codeOnly(kMsgDInfo);
  appendU16(message, 0);
  appendU16(message, 0);
  appendU16(message, 100);
  appendU16(message, 100);
  appendU16(message, 0);
  appendU16(message, 50);
  appendU16(message, 50);
  return message;
}

void verifyClientProxyDisconnected(const RecordingEventQueue &events, const FakeStream &stream)
{
  QVERIFY(stream.closed());
  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  QVERIFY(events.addedEvents().front().getType() == EventTypes::ClientProxyDisconnected);
}

Client *undereferenceableClient()
{
  // These paths must queue cleanup without calling through to Client.
  return reinterpret_cast<Client *>(0x1);
}

TestAppUtil &testAppUtil()
{
  static TestAppUtil util;
  return util;
}

const Client::DisconnectRequest *disconnectRequest(const RecordingEventQueue &events)
{
  if (events.addedEvents().size() != 1) {
    return nullptr;
  }
  return static_cast<const Client::DisconnectRequest *>(events.addedEvents().front().getDataObject());
}

} // namespace

void ServerProxyTests::initTestCase()
{
  m_arch.init();
  (void)testAppUtil();
  m_log.setFilter(LogLevel::Level::Debug);
}

void ServerProxyTests::foregroundFullscreen_reportsIdleTransitionsAndCleansUp()
{
  const auto oldTls = Settings::value(Settings::Security::TlsEnabled);
  const auto restoreTls = qScopeGuard([oldTls] { Settings::setValue(Settings::Security::TlsEnabled, oldTls); });
  Settings::setValue(Settings::Security::TlsEnabled, false);
  RecordingEventQueue events;
  auto *platform = new FakePlatformScreen(&events);
  deskflow::Screen screen(platform, &events);
  auto *factory = new FakeSocketFactory(&events);
  Client client(&events, "secondary", NetworkAddress("127.0.0.1", 24800), factory, &screen);
  client.connect();
  auto *socket = factory->socket;
  QVERIFY(socket != nullptr);
  QVERIFY(events.dispatchEvent(Event(EventTypes::DataSocketConnected, socket)));
  std::string hello = "Barrier";
  appendU16(hello, kProtocolMajorVersion);
  appendU16(hello, kProtocolMinorVersion);
  socket->pushPacket(hello);
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, socket)));
  QCOMPARE(client.protocolMinorVersion(), kProtocolMinorVersion);
  socket->pushPacket(codeOnly(kMsgQInfo));
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, socket)));
  QVERIFY(socket->stream.takeWritten().find(std::string(kMsgCForegroundFullscreen, 4) + char{0}) != std::string::npos);

  auto *timer = events.repeatingTimer();
  QVERIFY(timer != nullptr);
  QCOMPARE(events.repeatingDuration(), 0.1);
  platform->fullscreen = true;
  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, timer)));
  QVERIFY(socket->stream.takeWritten().find(std::string(kMsgCForegroundFullscreen, 4) + char{1}) != std::string::npos);
  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, timer)));
  QVERIFY(socket->stream.takeWritten().empty());

  platform->fullscreen = false;
  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, timer)));
  QVERIFY(socket->stream.takeWritten().find(std::string(kMsgCForegroundFullscreen, 4) + char{0}) != std::string::npos);
  client.disconnect(nullptr);
  QVERIFY(events.timerDeleted(timer));
  QVERIFY(!events.dispatchEvent(Event(EventTypes::Timer, timer)));
}

void ServerProxyTests::foregroundFullscreen_preservesLegacyProtocol()
{
  const auto oldTls = Settings::value(Settings::Security::TlsEnabled);
  const auto restoreTls = qScopeGuard([oldTls] { Settings::setValue(Settings::Security::TlsEnabled, oldTls); });
  Settings::setValue(Settings::Security::TlsEnabled, false);
  RecordingEventQueue events;
  deskflow::Screen screen(new FakePlatformScreen(&events), &events);
  auto *factory = new FakeSocketFactory(&events);
  Client client(&events, "secondary", NetworkAddress("127.0.0.1", 24800), factory, &screen);
  client.connect();
  auto *socket = factory->socket;
  QVERIFY(socket != nullptr);
  QVERIFY(events.dispatchEvent(Event(EventTypes::DataSocketConnected, socket)));
  std::string hello = "Barrier";
  appendU16(hello, kProtocolMajorVersion);
  appendU16(hello, 10);
  socket->pushPacket(hello);
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, socket)));
  QCOMPARE(client.protocolMinorVersion(), int16_t{10});
  socket->pushPacket(codeOnly(kMsgQInfo));
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, socket)));
  QVERIFY(socket->stream.takeWritten().find(codeOnly(kMsgCForegroundFullscreen)) == std::string::npos);
  QVERIFY(events.repeatingTimer() == nullptr);
}

void ServerProxyTests::handleKeepAliveAlarm_timeout_queuesDisconnectRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, events.timer())));

  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto &event = events.addedEvents().front();
  QVERIFY(event.getType() == EventTypes::ClientDisconnectRequested);
  QCOMPARE(event.getTarget(), stream.getEventTarget());

  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Disconnect);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("server is not responding"));
}

void ServerProxyTests::handleData_incompleteMessage_queuesDisconnectRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  stream.push("DF");
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto &event = events.addedEvents().front();
  QVERIFY(event.getType() == EventTypes::ClientDisconnectRequested);
  QCOMPARE(event.getTarget(), stream.getEventTarget());

  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Disconnect);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("incomplete message from server"));
}

void ServerProxyTests::handleData_largeBurst_queuesBoundedContinuation()
{
  RecordingEventQueue events;
  FakeStream stream;
  std::string messages;
  for (int i = 0; i < 64; ++i) {
    messages.append(kMsgCNoop, 4);
  }
  stream.push(messages);
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  QCOMPARE(stream.getSize(), uint32_t{32 * 4});
  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto &continuation = events.addedEvents().front();
  QVERIFY(continuation.getType() == EventTypes::StreamInputReady);
  QCOMPARE(continuation.getTarget(), stream.getEventTarget());
}

void ServerProxyTests::parseHandshakeMessage_mouseMoveBeforeOptions_isAccepted()
{
  RecordingEventQueue events;
  FakeStream stream;
  std::string payload;
  appendU16(payload, 320);
  appendU16(payload, 240);
  payload.push_back('x');
  stream.push(payload);
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(proxy.parseHandshakeMessageReturnsOkay(reinterpret_cast<const uint8_t *>(kMsgDMouseMove)));
  QCOMPARE(stream.getSize(), uint32_t{1});
}

void ServerProxyTests::parseHandshakeMessage_protocolError_queuesRefusalRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(proxy.parseHandshakeMessageReturnsDisconnect(reinterpret_cast<const uint8_t *>(kMsgEBad)));
  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Refuse);
  QVERIFY(request->refusalReason() == deskflow::core::ConnectionRefusal::ProtocolError);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("server reported a protocol error"));
}

void ServerProxyTests::handleData_truncatedHandshakePayload_queuesDisconnectRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  stream.push(codeOnly(kMsgEIncompatible));
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Disconnect);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("invalid message from server"));
}

void ServerProxyTests::parseMessage_truncatedPayload_data()
{
  QTest::addColumn<QByteArray>("code");

  QTest::newRow("absolute mouse motion") << QByteArray(kMsgDMouseMove, 4);
  QTest::newRow("relative mouse motion") << QByteArray(kMsgDMouseRelMove, 4);
  QTest::newRow("mouse wheel") << QByteArray(kMsgDMouseWheel, 4);
  QTest::newRow("key down") << QByteArray(kMsgDKeyDown, 4);
  QTest::newRow("key down with language") << QByteArray(kMsgDKeyDownLang, 4);
  QTest::newRow("key repeat") << QByteArray(kMsgDKeyRepeat, 4);
  QTest::newRow("key up") << QByteArray(kMsgDKeyUp, 4);
  QTest::newRow("mouse down") << QByteArray(kMsgDMouseDown, 4);
  QTest::newRow("mouse up") << QByteArray(kMsgDMouseUp, 4);
  QTest::newRow("enter") << QByteArray(kMsgCEnter, 4);
  QTest::newRow("clipboard ownership") << QByteArray(kMsgCClipboard, 4);
  QTest::newRow("screen saver") << QByteArray(kMsgCScreenSaver, 4);
  QTest::newRow("options") << QByteArray(kMsgDSetOptions, 4);
  QTest::newRow("secure input") << QByteArray(kMsgDSecureInputNotification, 4);
  QTest::newRow("input language control") << QByteArray(kMsgDInputLangControl, 4);
}

void ServerProxyTests::parseMessage_truncatedPayload()
{
  QFETCH(QByteArray, code);

  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY_THROWS_EXCEPTION(BadClientException, proxy.parseNormal(code));
  QVERIFY(events.addedEvents().empty());
}

void ServerProxyTests::parseHandshakeMessage_truncatedLanguagePayload_throws()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY_THROWS_EXCEPTION(BadClientException, proxy.parseHandshake(QByteArray(kMsgDLanguageSynchronisation, 4)));
  QVERIFY(events.addedEvents().empty());
}

void ServerProxyTests::parseMessage_allocationFailure_throwsBeforeSideEffects()
{
  RecordingEventQueue events;
  FakeStream stream;
  stream.failReadAfter(0);
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY_THROWS_EXCEPTION(BadClientException, proxy.parseNormal(QByteArray(kMsgDSecureInputNotification, 4)));
  QVERIFY(events.addedEvents().empty());
}

void ServerProxyTests::clientProxy_truncatedInitialInfo_disconnects()
{
  RecordingEventQueue events;
  auto *stream = new FakeStream;
  ClientProxy1_0 proxy("secondary", stream, &events);
  stream->push(codeOnly(kMsgDInfo));

  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream->getEventTarget())));
  verifyClientProxyDisconnected(events, *stream);
}

void ServerProxyTests::clientProxy_allocationFailure_disconnects()
{
  RecordingEventQueue events;
  auto *stream = new FakeStream;
  ClientProxy1_0 proxy("secondary", stream, &events);
  stream->push(codeOnly(kMsgDInfo));
  stream->failReadAfter(1);

  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream->getEventTarget())));
  verifyClientProxyDisconnected(events, *stream);
}

void ServerProxyTests::clientProxy_truncatedRuntimePayload_disconnects_data()
{
  QTest::addColumn<QByteArray>("code");

  QTest::newRow("clipboard ownership") << QByteArray(kMsgCClipboard, 4);
  QTest::newRow("input language status") << QByteArray(kMsgCInputLangStatus, 4);
  QTest::newRow("display layout") << QByteArray(kMsgCDisplayLayout, 4);
  QTest::newRow("foreground fullscreen") << QByteArray(kMsgCForegroundFullscreen, 4);
}

void ServerProxyTests::clientProxy_truncatedRuntimePayload_disconnects()
{
  QFETCH(QByteArray, code);

  RecordingEventQueue events;
  auto *stream = new FakeStream;
  ClientProxy1_11 proxy("secondary", stream, reinterpret_cast<Server *>(0x1), &events);

  stream->push(validInfoMessage());
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream->getEventTarget())));
  QVERIFY(!stream->closed());
  events.clearAddedEvents();

  stream->push({code.constData(), static_cast<size_t>(code.size())});
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream->getEventTarget())));
  verifyClientProxyDisconnected(events, *stream);
}

void ServerProxyTests::clientProxy_largeBurst_yieldsToTimerAndSecondClient()
{
  RecordingEventQueue events;
  auto *busyStream = new FakeStream;
  auto *otherStream = new FakeStream;
  ClientProxy1_0 busyProxy("busy", busyStream, &events);
  ClientProxy1_0 otherProxy("other", otherStream, &events);

  busyProxy.setOptions({kOptionHeartbeat, 1000});
  otherProxy.setOptions({kOptionHeartbeat, 1000});
  const size_t heartbeatTimersAfterSetup = events.oneShotTimerCreations();
  QCOMPARE(heartbeatTimersAfterSetup, static_cast<size_t>(2));

  std::string burst;
  for (int i = 0; i < 64; ++i) {
    burst.append(kMsgCNoop, 4);
  }
  busyStream->push(burst);
  otherStream->push(codeOnly(kMsgCNoop));

  bool timerHandled = false;
  int timerTarget = 0;
  events.addHandler(EventTypes::Timer, &timerTarget, [&](const auto &) { timerHandled = true; });
  events.addEvent(Event(EventTypes::StreamInputReady, busyStream->getEventTarget()));
  events.addEvent(Event(EventTypes::Timer, &timerTarget));
  events.addEvent(Event(EventTypes::StreamInputReady, otherStream->getEventTarget()));

  QVERIFY(events.dispatchNextAddedEvent());
  QCOMPARE(busyStream->getSize(), uint32_t{32 * 4});
  QCOMPARE(events.oneShotTimerCreations(), heartbeatTimersAfterSetup + 1);
  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(3));

  QVERIFY(events.dispatchNextAddedEvent());
  QVERIFY(timerHandled);
  QCOMPARE(otherStream->getSize(), uint32_t{4});

  QVERIFY(events.dispatchNextAddedEvent());
  QCOMPARE(otherStream->getSize(), uint32_t{0});
  QCOMPARE(events.oneShotTimerCreations(), heartbeatTimersAfterSetup + 2);

  QVERIFY(events.dispatchNextAddedEvent());
  QCOMPARE(busyStream->getSize(), uint32_t{0});
  QCOMPARE(events.oneShotTimerCreations(), heartbeatTimersAfterSetup + 3);
  QVERIFY(events.addedEvents().empty());

  events.removeHandler(EventTypes::Timer, &timerTarget);
}

QTEST_MAIN(ServerProxyTests)
