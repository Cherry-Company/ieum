/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TailscaleIntegrationTests.h"

#include "common/TailscaleIntegration.h"

#include <QSignalSpy>
#include <QTimer>

#include <memory>
#include <utility>
#include <vector>

using deskflow::network::TailscaleIntegration;
using deskflow::network::TailscaleState;
using deskflow::network::TailscaleStatus;

namespace {

const auto runningStatusJson = QByteArrayLiteral(R"json(
  {
    "BackendState": "Running",
    "TailscaleIPs": ["100.68.129.102"],
    "Peer": {}
  }
)json");

struct FakeProcessState
{
  TailscaleIntegration::ProcessCallbacks callbacks;
  QByteArray standardOutput;
  QByteArray standardError;
  int exitCode = 0;
  bool killed = false;

  void start()
  {
    if (callbacks.started) {
      callbacks.started();
    }
  }

  void finish()
  {
    if (callbacks.finished) {
      callbacks.finished();
    }
  }
};

class FakeProcess final : public TailscaleIntegration::Process
{
public:
  explicit FakeProcess(std::shared_ptr<FakeProcessState> state) : m_state(std::move(state))
  {
  }

  ~FakeProcess() override
  {
    m_state->callbacks = {};
  }

  void setCallbacks(TailscaleIntegration::ProcessCallbacks callbacks) override
  {
    m_state->callbacks = std::move(callbacks);
  }

  void start(const QString &, const QStringList &) override
  {
  }

  void kill() override
  {
    m_state->killed = true;
  }

  QByteArray readAllStandardOutput() override
  {
    return m_state->standardOutput;
  }

  QByteArray readAllStandardError() override
  {
    return m_state->standardError;
  }

  int exitCode() const override
  {
    return m_state->exitCode;
  }

private:
  std::shared_ptr<FakeProcessState> m_state;
};

class FakeProcessFactory
{
public:
  TailscaleIntegration::ProcessFactory makeFactory()
  {
    return [this] {
      auto state = std::make_shared<FakeProcessState>();
      m_states.push_back(state);
      return std::make_unique<FakeProcess>(state);
    };
  }

  std::shared_ptr<FakeProcessState> state(int index) const
  {
    return m_states.at(static_cast<size_t>(index));
  }

  int count() const
  {
    return static_cast<int>(m_states.size());
  }

private:
  std::vector<std::shared_ptr<FakeProcessState>> m_states;
};

TailscaleIntegration makeIntegration(FakeProcessFactory &factory)
{
  return TailscaleIntegration(factory.makeFactory(), [] { return QStringLiteral("fake-tailscale"); });
}

TailscaleStatus takeStatus(QSignalSpy &spy)
{
  return qvariant_cast<TailscaleStatus>(spy.takeFirst().at(0));
}

} // namespace

void TailscaleIntegrationTests::parsesRunningStatus()
{
  const auto json = QByteArrayLiteral(R"json(
    {
      "Version": "1.98.4",
      "BackendState": "Running",
      "TailscaleIPs": ["fd7a:115c:a1e0::1", "100.68.129.102"],
      "Self": {"DNSName": "this-pc.example.ts.net."},
      "Peer": {
        "nodekey:mac": {
          "ID": "mac-id",
          "DNSName": "studio-mac.example.ts.net.",
          "HostName": "Mac Studio",
          "OS": "macOS",
          "TailscaleIPs": ["fd7a:115c:a1e0::2", "100.92.158.1"],
          "Online": true
        },
        "nodekey:phone": {
          "ID": "phone-id",
          "DNSName": "phone.example.ts.net.",
          "OS": "iOS",
          "TailscaleIPs": ["100.85.45.87"],
          "Online": true
        },
        "nodekey:offline": {
          "ID": "offline-id",
          "DNSName": "offline-pc.example.ts.net.",
          "OS": "windows",
          "TailscaleIPs": ["100.117.91.86"],
          "Online": false
        }
      }
    }
  )json");

  const auto status = TailscaleIntegration::parseStatus(json);
  QCOMPARE(status.state, TailscaleState::Running);
  QVERIFY(status.isReady());
  QCOMPARE(status.localDnsName, QStringLiteral("this-pc.example.ts.net"));
  QCOMPARE(status.preferredLocalAddress(), QStringLiteral("100.68.129.102"));

  const auto peers = status.onlineDesktopPeers();
  QCOMPARE(peers.size(), 1);
  QCOMPARE(peers.first().name, QStringLiteral("studio-mac"));
  QCOMPARE(peers.first().preferredAddress(), QStringLiteral("100.92.158.1"));
}

void TailscaleIntegrationTests::parsesLoginState()
{
  const auto status =
      TailscaleIntegration::parseStatus(QByteArrayLiteral(R"json({"BackendState":"NeedsLogin","Peer":{}})json"));
  QCOMPARE(status.state, TailscaleState::NeedsLogin);
  QVERIFY(!status.isReady());
}

void TailscaleIntegrationTests::parsesMachineAuthorizationState()
{
  const auto status =
      TailscaleIntegration::parseStatus(QByteArrayLiteral(R"json({"BackendState":"NeedsMachineAuth","Peer":{}})json"));
  QCOMPARE(status.state, TailscaleState::NeedsLogin);
  QVERIFY(!status.isReady());
}

void TailscaleIntegrationTests::rejectsMalformedStatus()
{
  const auto status = TailscaleIntegration::parseStatus(QByteArrayLiteral("{not-json"));
  QCOMPARE(status.state, TailscaleState::Error);
  QVERIFY(!status.error.isEmpty());
}

void TailscaleIntegrationTests::queryKeepsEventLoopResponsive()
{
  FakeProcessFactory factory;
  auto integration = makeIntegration(factory);
  bool eventProcessed = false;

  integration.query(1000);
  QTimer::singleShot(0, &integration, [&eventProcessed] { eventProcessed = true; });

  QTRY_VERIFY_WITH_TIMEOUT(eventProcessed, 100);
  QVERIFY(integration.isQuerying());
}

void TailscaleIntegrationTests::hungQueryTimesOut()
{
  FakeProcessFactory factory;
  auto integration = makeIntegration(factory);
  QSignalSpy finishedSpy(&integration, &TailscaleIntegration::queryFinished);

  integration.query(40);

  QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 250);
  const auto status = takeStatus(finishedSpy);
  QVERIFY(status.error.contains(QStringLiteral("timed out"), Qt::CaseInsensitive));
  QVERIFY(factory.state(0)->killed);
  QVERIFY(!integration.isQuerying());
}

void TailscaleIntegrationTests::duplicateQueriesAreCoalesced()
{
  FakeProcessFactory factory;
  auto integration = makeIntegration(factory);
  QSignalSpy startedSpy(&integration, &TailscaleIntegration::queryStarted);
  QSignalSpy finishedSpy(&integration, &TailscaleIntegration::queryFinished);

  integration.query();
  integration.query();
  QCOMPARE(startedSpy.count(), 1);
  QCOMPARE(factory.count(), 1);

  factory.state(0)->standardOutput = runningStatusJson;
  factory.state(0)->start();
  factory.state(0)->finish();

  QCOMPARE(finishedSpy.count(), 1);
  QCOMPARE(takeStatus(finishedSpy).state, TailscaleState::Running);
}

void TailscaleIntegrationTests::lateCompletionDoesNotReplaceNewerQuery()
{
  FakeProcessFactory factory;
  auto integration = makeIntegration(factory);
  QSignalSpy finishedSpy(&integration, &TailscaleIntegration::queryFinished);

  integration.query();
  const auto lateCompletion = factory.state(0)->callbacks.finished;
  integration.cancel();
  integration.query();

  lateCompletion();
  QCOMPARE(finishedSpy.count(), 0);

  factory.state(1)->standardOutput = runningStatusJson;
  factory.state(1)->start();
  factory.state(1)->finish();

  QCOMPARE(finishedSpy.count(), 1);
  QCOMPARE(takeStatus(finishedSpy).preferredLocalAddress(), QStringLiteral("100.68.129.102"));
}

void TailscaleIntegrationTests::destructionIgnoresLateCompletion()
{
  FakeProcessFactory factory;
  int resultCount = 0;
  std::function<void()> lateCompletion;

  {
    auto integration =
        std::make_unique<TailscaleIntegration>(factory.makeFactory(), [] { return QStringLiteral("fake-tailscale"); });
    connect(integration.get(), &TailscaleIntegration::queryFinished, [&resultCount] { ++resultCount; });
    integration->query();
    lateCompletion = factory.state(0)->callbacks.finished;
  }

  lateCompletion();
  QCoreApplication::processEvents();
  QCOMPARE(resultCount, 0);
  QVERIFY(factory.state(0)->killed);
}

void TailscaleIntegrationTests::completedQueryParsesStatus()
{
  FakeProcessFactory factory;
  auto integration = makeIntegration(factory);
  QSignalSpy finishedSpy(&integration, &TailscaleIntegration::queryFinished);

  integration.query();
  factory.state(0)->standardOutput = runningStatusJson;
  factory.state(0)->start();
  factory.state(0)->finish();

  QCOMPARE(finishedSpy.count(), 1);
  const auto status = takeStatus(finishedSpy);
  QCOMPARE(status.state, TailscaleState::Running);
  QCOMPARE(status.preferredLocalAddress(), QStringLiteral("100.68.129.102"));
}

QTEST_MAIN(TailscaleIntegrationTests)
