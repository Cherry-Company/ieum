// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "common/IpcMessage.h"
#include "common/Settings.h"
#include "gui/session/SessionController.h"
#include "gui/session/SessionWindow.h"
#include <QDir>
#include <QJsonArray>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>
#include <QUuid>
#include <deque>

using namespace deskflow::gui::session;
namespace pro = deskflow::professional;
class FakeSessionPlatform final : public SessionPlatform
{
public:
  int captureStarts = 0, captureStops = 0, screenReads = 0, releases = 0, locks = 0, wakes = 0;
  bool capture = false, playing = false, isLocked = false, permitScreen = true;
  std::optional<InputMode> mode;
  QList<Input> injected;
  QByteArray audio;
  QImage screen() override
  {
    ++screenReads;
    QImage image(160, 90, QImage::Format_RGB32);
    image.fill(Qt::blue);
    return image;
  }
  bool requestScreenPermission() override
  {
    return permitScreen;
  }
  bool startAudioCapture() override
  {
    ++captureStarts;
    capture = true;
    return true;
  }
  void stopAudioCapture() override
  {
    ++captureStops;
    capture = false;
  }
  bool playAudio(const QByteArray &pcm) override
  {
    playing = true;
    audio += pcm;
    return true;
  }
  void stopAudioPlayback() override
  {
    playing = false;
  }
  bool startInputCapture(InputMode value) override
  {
    mode = value;
    return true;
  }
  void stopInputCapture() override
  {
    mode.reset();
  }
  bool inject(const Input &input) override
  {
    injected.append(input);
    return true;
  }
  void releaseInput() override
  {
    ++releases;
  }
  bool lockDesktop() override
  {
    ++locks;
    isLocked = true;
    return true;
  }
  bool wakeDisplays() override
  {
    ++wakes;
    return true;
  }
  bool locked() override
  {
    return isLocked;
  }
};

class SessionMesh : public QObject
{
public:
  explicit SessionMesh(int count = 2)
  {
    for (int i = 0; i < count; ++i) {
      connections.append(QUuid::createUuid().toString(QUuid::WithoutBraces));
      auto platform = std::make_unique<FakeSessionPlatform>();
      platforms.push_back(platform.get());
      SessionController::Transport transport{
          .send =
              [this, i](const QByteArray &bytes) {
                auto decoded = pro::decode(bytes);
                if (!decoded)
                  return false;
                decoded->source = name(i);
                if (decoded->operation == "open")
                  decoded->body["connection"] = connections[i];
                if (drop && drop(*decoded))
                  return true;
                if (decoded->operation == "discover") {
                  QJsonArray peers;
                  for (int n = 0; n < static_cast<int>(controllers.size()); ++n)
                    if (n != i)
                      peers.append(name(n));
                  pro::Message reply{"hub", name(i), "peers", {}, {}, {{"peers", peers}}};
                  const auto packet = pro::encode(reply);
                  QTimer::singleShot(0, this, [this, i, packet] { controllers[i]->receive(packet); });
                } else {
                  for (int n = 0; n < static_cast<int>(controllers.size()); ++n)
                    if (decoded->target == name(n)) {
                      const auto packet = pro::encode(*decoded);
                      QTimer::singleShot(0, this, [this, n, packet] { controllers[n]->receive(packet); });
                    }
                }
                return true;
              },
          .localInputMode =
              [this, i](bool mode) {
                modes[i] = mode;
                if (acknowledgeInput)
                  QTimer::singleShot(0, this, [this, i, mode] { controllers[i]->inputModeReady(mode); });
              },
          .authorized = [this](const QString &) { return licensed; },
          .now = [this] { return time; }
      };
      controllers.push_back(std::make_unique<SessionController>(transport, std::move(platform)));
    }
    for (int i = 0; i < count; ++i)
      controllers[i]->setOnline(true, name(i));
  }
  QString name(int i) const
  {
    return QStringLiteral("computer-%1").arg(i);
  }
  std::vector<std::unique_ptr<SessionController>> controllers;
  std::vector<FakeSessionPlatform *> platforms;
  QHash<int, bool> modes;
  bool licensed = true;
  bool acknowledgeInput = true;
  QStringList connections;
  std::function<bool(const pro::Message &)> drop;
  qint64 time = 1000;
};

class ProfessionalSessionTests final : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase()
  {
    QVERIFY(m_temp.isValid());
    Settings::setSettingsFile(m_temp.filePath("settings.ini"));
  }
  void init()
  {
    Settings::setValue("Professional/PermissionGeneration", QUuid::createUuid().toString());
  }
  void codecRejectsUnboundedAndAmbiguousEnvelopes()
  {
    pro::Message message{"source", "target", "open", "remote-viewing", "b5d9fa8b-246b-439b-a04f-c1f2f3874799", {}};
    const auto encoded = pro::encode(message);
    QVERIFY(!encoded.isEmpty());
    QVERIFY(pro::decode(encoded));
    QVERIFY(!pro::decode(QByteArray(pro::kMaxMessageBytes + 1, 'x')));
    QVERIFY(!pro::decode("[]"));
    QVERIFY(!pro::decode("{}"));
    auto invalid = message;
    invalid.operation = "execute";
    QVERIFY(pro::encode(invalid).isEmpty());
    invalid = message;
    invalid.session = "shared";
    QVERIFY(pro::encode(invalid).isEmpty());
    invalid = message;
    invalid.target = QString(129, 'x');
    QVERIFY(pro::encode(invalid).isEmpty());
    invalid = message;
    invalid.source = "bad\nname";
    QVERIFY(pro::encode(invalid).isEmpty());
    invalid = message;
    invalid.body = {{"data", QString(pro::kMaxMessageBytes, 'x')}};
    QVERIFY(pro::encode(invalid).isEmpty());
    QCOMPARE(deskflow::ipc::sanitizeMessageForLog("professional=SECRET"), QString("professional=<redacted>"));
  }
  void frameAssemblyRejectsGapsReplaysAndOversizedAllocation()
  {
    FrameAssembly frame;
    QVERIFY(!frame.append(1, pro::kMaxFrameBytes + 1, 0, "x"));
    QVERIFY(!frame.append(1, 4, 1, "x"));
    QVERIFY(frame.append(1, 4, 0, "ab"));
    QVERIFY(!frame.append(1, 4, 0, "ab"));
    QVERIFY(!frame.append(1, 4, 3, "d"));
    QVERIFY(frame.append(1, 4, 2, "cd"));
    QVERIFY(frame.complete());
    QCOMPARE(frame.take(), QByteArray("abcd"));
    QVERIFY(!frame.append(1, 4, 0, "abcd"));
    QVERIFY(frame.append(2, 1, 0, "z"));
    QCOMPARE(frame.take(), QByteArray("z"));
  }
  void inputCodecRejectsInvalidCoordinatesAndKeys()
  {
    const Input key{Input::Kind::Key, 0x1e, true};
    QVERIFY(Input::fromJson(key.toJson()));
    auto invalid = key.toJson();
    invalid["code"] = 0;
    QVERIFY(!Input::fromJson(invalid));
    invalid = key.toJson();
    invalid["code"] = 65535;
    QVERIFY(!Input::fromJson(invalid));
    invalid = key.toJson();
    invalid["kind"] = 1.5;
    QVERIFY(!Input::fromJson(invalid));
    const Input motion{Input::Kind::Move, 0, false, 65535, 65535};
    QVERIFY(Input::fromJson(motion.toJson()));
    invalid = motion.toJson();
    invalid["x"] = -1;
    QVERIFY(!Input::fromJson(invalid));
    invalid = motion.toJson();
    invalid["down"] = true;
    QVERIFY(!Input::fromJson(invalid));
  }
  void noCaptureBeforeConsentAndScreenStopsWhenClosed()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested), frames(&a, &SessionController::frameReceived);
    const auto id = a.request(mesh.name(1), "remote-viewing");
    QVERIFY(!id.isEmpty());
    QTRY_COMPARE(consent.size(), 1);
    QCOMPARE(mesh.platforms[1]->screenReads, 0);
    QCOMPARE(b.activeCount(), 0);
    b.consent(id, true);
    QTRY_COMPARE(a.activeCount(), 1);
    QTRY_VERIFY(!frames.isEmpty());
    QCOMPARE(frames.first().at(2).value<QImage>().size(), QSize(160, 90));
    a.stop(id);
    QTRY_COMPARE(b.activeCount(), 0);
    const auto reads = mesh.platforms[1]->screenReads;
    QTest::qWait(250);
    QCOMPARE(mesh.platforms[1]->screenReads, reads);
  }
  void deniedPermissionAndExpiredPlanNeverStartCapture()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    const auto id = a.request(mesh.name(1), "remote-viewing");
    QTRY_COMPARE(consent.size(), 1);
    mesh.platforms[1]->permitScreen = false;
    b.consent(id, true);
    QTRY_COMPARE(a.activeCount(), 0);
    QCOMPARE(mesh.platforms[1]->screenReads, 0);
    mesh.licensed = false;
    QVERIFY(a.request(mesh.name(1), "audio-sharing").isEmpty());
  }
  void audioUsesSystemCaptureOnlyAfterConsentAndStopsOnDisconnect()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    const auto id = a.request(mesh.name(1), "audio-sharing");
    QTRY_COMPARE(consent.size(), 1);
    QVERIFY(!mesh.platforms[1]->capture);
    b.consent(id, true);
    QTRY_COMPARE(a.activeCount(), 1);
    QVERIFY(mesh.platforms[1]->capture);
    const QByteArray pcm(3840, '\x12');
    Q_EMIT mesh.platforms[1]->audioCaptured(pcm);
    QTRY_COMPARE(mesh.platforms[0]->audio, pcm);
    a.setOnline(false, mesh.name(0));
    QTRY_COMPARE(b.activeCount(), 0);
    QVERIFY(!mesh.platforms[1]->capture);
    QVERIFY(!mesh.platforms[0]->playing);
  }
  void inputSequenceGapAndLocalTakeoverReleaseHeldKeys()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    const auto id = a.request(mesh.name(1), "bidirectional-control");
    QTRY_COMPARE(consent.size(), 1);
    b.consent(id, true);
    QTRY_VERIFY(mesh.platforms[0]->mode == SessionPlatform::InputMode::Control);
    Q_EMIT mesh.platforms[0]->inputCaptured((Input{Input::Kind::Key, 0x1e, true}));
    QTRY_COMPARE(mesh.platforms[1]->injected.size(), 1);
    const auto releases = mesh.platforms[1]->releases;
    b.receive(
        pro::encode(
            {mesh.name(0),
             mesh.name(1),
             "input",
             "bidirectional-control",
             id,
             {{"seq", 3}, {"event", Input{Input::Kind::Key, 0x1e, false}.toJson()}}}
        )
    );
    QCOMPARE(b.activeCount(), 0);
    QVERIFY(mesh.platforms[1]->releases > releases);
    QTRY_COMPARE(a.activeCount(), 0);
    QVERIFY(!mesh.modes[0]);
    const auto second = a.request(mesh.name(1), "bidirectional-control");
    QTRY_COMPARE(consent.size(), 2);
    b.consent(second, true);
    QTRY_VERIFY(mesh.platforms[1]->mode == SessionPlatform::InputMode::Observe);
    Q_EMIT mesh.platforms[1]->inputCaptured((Input{Input::Kind::Button, 1, true}));
    QTRY_COMPARE(a.activeCount(), 0);
    QCOMPARE(b.activeCount(), 0);
  }
  void simultaneousMouseAndKeysReachEveryConsentingPeer()
  {
    SessionMesh mesh(3);
    auto &a = *mesh.controllers[0];
    QTRY_COMPARE(a.peers().size(), 2);
    QSignalSpy first(mesh.controllers[1].get(), &SessionController::consentRequested),
        second(mesh.controllers[2].get(), &SessionController::consentRequested);
    const auto one = a.request(mesh.name(1), "simultaneous-input"), two = a.request(mesh.name(2), "simultaneous-input");
    QTRY_COMPARE(first.size(), 1);
    QTRY_COMPARE(second.size(), 1);
    mesh.controllers[1]->consent(one, true);
    mesh.controllers[2]->consent(two, true);
    QTRY_COMPARE(a.activeCount(), 2);
    QTRY_VERIFY(mesh.platforms[0]->mode == SessionPlatform::InputMode::Broadcast);
    for (const Input event :
         {Input{Input::Kind::Move, 0, false, 100, 200}, Input{Input::Kind::Button, 1, true},
          Input{Input::Kind::Key, 0x1e, true}, Input{Input::Kind::Wheel, 0, false, 0, 120},
          Input{Input::Kind::Key, 0x1e, false}, Input{Input::Kind::Button, 1, false}})
      Q_EMIT mesh.platforms[0]->inputCaptured(event);
    QTRY_COMPARE(mesh.platforms[1]->injected.size(), 6);
    QTRY_COMPARE(mesh.platforms[2]->injected.size(), 6);
    Q_EMIT mesh.platforms[0]->inputCaptured((Input{Input::Kind::Escape}));
    QTRY_COMPARE(a.activeCount(), 0);
    QTRY_COMPARE(mesh.controllers[1]->activeCount(), 0);
    QTRY_COMPARE(mesh.controllers[2]->activeCount(), 0);
  }
  void heartbeatTimeoutAndLicenseExpiryReleaseInput()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    const auto id = a.request(mesh.name(1), "bidirectional-control");
    QTRY_COMPARE(consent.size(), 1);
    b.consent(id, true);
    QTRY_COMPARE(a.activeCount(), 1);
    const auto releases = mesh.platforms[1]->releases;
    mesh.time += 6100;
    b.tick();
    QCOMPARE(b.activeCount(), 0);
    QVERIFY(mesh.platforms[1]->releases > releases);
    QTRY_COMPARE(a.activeCount(), 0);
    const auto second = a.request(mesh.name(1), "bidirectional-control");
    QTRY_COMPARE(consent.size(), 2);
    b.consent(second, true);
    QTRY_COMPARE(a.activeCount(), 1);
    mesh.licensed = false;
    a.tick();
    QTRY_COMPARE(b.activeCount(), 0);
    QCOMPARE(a.activeCount(), 0);
  }
  void lockSyncDoesNotUnlockAndUnsolicitedCommandsAreIgnored()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    const auto id = a.request(mesh.name(1), "coordinated-lock-wake");
    QTRY_COMPARE(consent.size(), 1);
    b.consent(id, true);
    QTRY_COMPARE(a.activeCount(), 1);
    a.lockTogether();
    QTRY_COMPARE(mesh.platforms[1]->locks, 1);
    QCOMPARE(mesh.platforms[0]->locks, 1);
    b.receive(pro::encode({mesh.name(0), mesh.name(1), "wake", "coordinated-lock-wake", id, {}}));
    QCOMPARE(mesh.platforms[1]->wakes, 1);
    QVERIFY(mesh.platforms[1]->isLocked);
    b.receive(pro::encode({"untrusted", mesh.name(1), "lock", "coordinated-lock-wake", id, {}}));
    QCOMPARE(mesh.platforms[1]->locks, 1);
  }
  void rememberedConsentCanBeRevokedAndPendingRequestsExpire()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    const auto first = a.request(mesh.name(1), "audio-sharing");
    QTRY_COMPARE(consent.size(), 1);
    b.consent(first, true, true);
    QTRY_COMPARE(a.activeCount(), 1);
    a.stopAll();
    QTRY_COMPARE(b.activeCount(), 0);
    QVERIFY(!a.request(mesh.name(1), "audio-sharing").isEmpty());
    QTRY_COMPARE(a.activeCount(), 1);
    QCOMPARE(consent.size(), 1);
    b.revokePermissions();
    QTRY_COMPARE(a.activeCount(), 0);
    const auto pending = a.request(mesh.name(1), "audio-sharing");
    QTRY_COMPARE(consent.size(), 2);
    QVERIFY(!mesh.platforms[1]->capture);
    mesh.time += 61000;
    b.tick();
    b.consent(pending, true);
    QCOMPARE(b.activeCount(), 0);
    QVERIFY(!mesh.platforms[1]->capture);
  }
  void lostAudioAcknowledgmentsBoundTransmissionAndEndCapture()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    const auto id = a.request(mesh.name(1), "audio-sharing");
    QTRY_COMPARE(consent.size(), 1);
    b.consent(id, true);
    QTRY_COMPARE(a.activeCount(), 1);
    mesh.drop = [](const pro::Message &message) { return message.operation == "ack"; };
    const QByteArray pcm(3840, 1);
    for (int i = 0; i < 100; ++i)
      Q_EMIT mesh.platforms[1]->audioCaptured(pcm);
    QTRY_COMPARE(mesh.platforms[0]->audio.size(), pcm.size() * 4);
    mesh.time += 3100;
    b.receive(pro::encode({mesh.name(0), mesh.name(1), "ping", "audio-sharing", id, {}}));
    b.tick();
    QCOMPARE(b.activeCount(), 0);
    QVERIFY(!mesh.platforms[1]->capture);
    QTRY_COMPARE(a.activeCount(), 0);
  }
  void desktopLockStopsMediaAndInputOwnershipRequiresCoreAcknowledgment()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    const auto audio = a.request(mesh.name(1), "audio-sharing");
    QTRY_COMPARE(consent.size(), 1);
    b.consent(audio, true);
    QTRY_COMPARE(a.activeCount(), 1);
    mesh.platforms[1]->isLocked = true;
    b.tick();
    QTRY_COMPARE(a.activeCount(), 0);
    QVERIFY(!mesh.platforms[1]->capture);
    mesh.platforms[1]->isLocked = false;
    mesh.acknowledgeInput = false;
    const auto control = a.request(mesh.name(1), "bidirectional-control");
    QTRY_COMPARE(consent.size(), 2);
    b.consent(control, true);
    QTRY_COMPARE(a.activeCount(), 1);
    QVERIFY(!mesh.platforms[0]->mode);
    QVERIFY(!mesh.platforms[1]->mode);
    mesh.time += 3100;
    a.tick();
    QTRY_COMPARE(b.activeCount(), 0);
    QVERIFY(!mesh.platforms[0]->mode);
  }
  void hubOnlyPausesForTheMatchingReceiversAcceptance()
  {
    pro::InputLeaseBook book;
    pro::Message request{"secondary-a", "secondary-b", "open", "bidirectional-control", "c1", {}};
    book.observe(request, 1000);
    QVERIFY(!book.active(1000));
    auto accept = request;
    accept.operation = "accept";
    book.observe(accept, 1001);
    QVERIFY(!book.active(1001));
    accept.source = "unrelated";
    accept.target = request.source;
    book.observe(accept, 1002);
    QVERIFY(!book.active(1002));
    accept.source = request.target;
    book.observe(accept, 1003);
    QVERIFY(book.active(1003));
    auto ping = request;
    ping.operation = "ping";
    book.observe(ping, 6000);
    QVERIFY(book.active(11000));
    QVERIFY(!book.active(12000));
    book.observe(request, 13000);
    book.observe(accept, 13001);
    QVERIFY(book.active(13001));
    auto close = request;
    close.operation = "close";
    close.source = "unrelated";
    book.observe(close, 13002);
    QVERIFY(book.active(13002));
    close.source = request.source;
    book.observe(close, 13003);
    QVERIFY(!book.active(13003));
    book.observe(request, 14000);
    book.observe(accept, 14001);
    book.forget(request.target);
    QVERIFY(!book.active(14002));
  }
  void sharingPanelStartsAndStopsTheSelectedFeature()
  {
    const auto originalPalette = qApp->palette();
    if (qEnvironmentVariable("IEUM_TEST_DARK") == "1") {
      QPalette dark = originalPalette;
      dark.setColor(QPalette::Window, QColor("#1c1e23"));
      dark.setColor(QPalette::Base, QColor("#292c33"));
      dark.setColor(QPalette::WindowText, QColor("#edf0f5"));
      dark.setColor(QPalette::Text, QColor("#edf0f5"));
      dark.setColor(QPalette::Button, QColor("#292c33"));
      dark.setColor(QPalette::ButtonText, QColor("#edf0f5"));
      dark.setColor(QPalette::Highlight, QColor("#8fbaff"));
      dark.setColor(QPalette::HighlightedText, QColor("#10151e"));
      qApp->setPalette(dark);
    }
    QTranslator translator;
    if (!qEnvironmentVariable("IEUM_TEST_TRANSLATION").isEmpty()) {
      QVERIFY(translator.load(qEnvironmentVariable("IEUM_TEST_TRANSLATION")));
      qApp->installTranslator(&translator);
    }
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    SessionWindow window(&a);
    window.show();
    auto *audio = window.findChild<QPushButton *>("sharing-audio-sharing");
    QVERIFY(audio);
    QTest::mouseClick(audio, Qt::LeftButton);
    QTRY_COMPARE(consent.size(), 1);
    b.consent(consent.first().first().toString(), true);
    QTRY_COMPARE(a.activeCount(), 1);
    const auto folder = qEnvironmentVariable("IEUM_TEST_SCREENSHOT_DIR");
    if (!folder.isEmpty()) {
      QVERIFY(QDir().mkpath(folder));
      QTest::qWait(50);
      QVERIFY(window.grab().save(folder + "/workspace-sharing-ko.png"));
      window.resize(500, 690);
      QTest::qWait(50);
      QVERIFY(window.grab().save(folder + "/workspace-sharing-narrow-ko.png"));
    }
    auto *stop = window.findChild<QPushButton *>("stopAllSharing");
    QVERIFY(stop);
    QTest::mouseClick(stop, Qt::LeftButton);
    QTRY_COMPARE(a.activeCount(), 0);
    QTRY_COMPARE(b.activeCount(), 0);
    qApp->removeTranslator(&translator);
    qApp->setPalette(originalPalette);
  }
  void sameNameNewConnectionCannotInheritSavedConsent()
  {
    SessionMesh mesh;
    auto &a = *mesh.controllers[0];
    auto &b = *mesh.controllers[1];
    QTRY_COMPARE(a.peers().size(), 1);
    QSignalSpy consent(&b, &SessionController::consentRequested);
    const auto first = a.request(mesh.name(1), "audio-sharing");
    QTRY_COMPARE(consent.size(), 1);
    b.consent(first, true, true);
    QTRY_COMPARE(a.activeCount(), 1);
    a.stopAll();
    QTRY_COMPARE(b.activeCount(), 0);
    mesh.connections[0] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVERIFY(!a.request(mesh.name(1), "audio-sharing").isEmpty());
    QTRY_COMPARE(consent.size(), 2);
    QCOMPARE(b.activeCount(), 0);
    QVERIFY(!mesh.platforms[1]->capture);
  }
  void routerNeverDeliversOldMediaToAReconnectedName()
  {
    pro::SessionRouteBook book;
    pro::Message open{"a", "b", "open", "audio-sharing", "session", {}};
    QVERIFY(book.observe(open, "connection-a", "connection-b", 1000));
    auto audio = open;
    audio.source = "b";
    audio.target = "a";
    audio.operation = "audio";
    QVERIFY(!book.observe(audio, "connection-b", "connection-a", 1001));
    auto accept = audio;
    accept.operation = "accept";
    QVERIFY(!book.observe(accept, "replacement-b", "connection-a", 1002));
    QVERIFY(book.observe(accept, "connection-b", "connection-a", 1003));
    QVERIFY(book.observe(audio, "connection-b", "connection-a", 1004));
    QVERIFY(!book.observe(audio, "connection-b", "replacement-a", 1005));
    QVERIFY(!book.observe(audio, "replacement-b", "connection-a", 1005));
    auto wrongFeature = audio;
    wrongFeature.feature = "remote-viewing";
    QVERIFY(!book.observe(wrongFeature, "connection-b", "connection-a", 1006));
    book.forget("a");
    QVERIFY(!book.observe(audio, "connection-b", "connection-a", 1007));
    QVERIFY(book.observe(open, "replacement-a", "connection-b", 1008));
    QVERIFY(!book.observe(accept, "connection-b", "connection-a", 1009));
    QVERIFY(book.observe(accept, "connection-b", "replacement-a", 1010));
    QVERIFY(!book.observe(audio, "connection-b", "replacement-a", 7010));
  }

private:
  QTemporaryDir m_temp;
};
QTEST_MAIN(ProfessionalSessionTests)
#include "ProfessionalSessionTests.moc"
