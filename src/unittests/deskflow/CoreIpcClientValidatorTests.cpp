/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/win32/CoreIpcClientValidator.h"
#include "deskflow/win32/CoreIpcClientValidatorDetail.h"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTest>
#include <QUuid>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>

namespace {

class UniqueHandle final
{
public:
  explicit UniqueHandle(HANDLE handle = nullptr) noexcept : m_handle(handle)
  {
  }

  ~UniqueHandle()
  {
    if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE) {
      CloseHandle(m_handle);
    }
  }

  UniqueHandle(const UniqueHandle &) = delete;
  UniqueHandle &operator=(const UniqueHandle &) = delete;

  [[nodiscard]] HANDLE get() const noexcept
  {
    return m_handle;
  }

private:
  HANDLE m_handle;
};

std::array<BYTE, SECURITY_MAX_SID_SIZE> interactiveSid()
{
  std::array<BYTE, SECURITY_MAX_SID_SIZE> sid{};
  DWORD size = static_cast<DWORD>(sid.size());
  if (CreateWellKnownSid(WinInteractiveSid, nullptr, sid.data(), &size) == FALSE) {
    return {};
  }
  return sid;
}

QString uniqueSocketName()
{
  return QStringLiteral("ieum-validator-test-%1-%2")
      .arg(QCoreApplication::applicationPid())
      .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

} // namespace

class CoreIpcClientValidatorTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void convertsRealProcessPrimaryTokenForMembership();
  void obtainsRealClientPidFromServerSideLocalSocket();
};

void CoreIpcClientValidatorTests::convertsRealProcessPrimaryTokenForMembership()
{
  const auto sid = interactiveSid();
  QVERIFY(IsValidSid(const_cast<BYTE *>(sid.data())) != FALSE);

  HANDLE primaryTokenRaw = nullptr;
  QVERIFY(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &primaryTokenRaw) != FALSE);
  const UniqueHandle primaryToken(primaryTokenRaw);

  BOOL effectiveTokenIsInteractive = FALSE;
  QVERIFY(CheckTokenMembership(nullptr, const_cast<BYTE *>(sid.data()), &effectiveTokenIsInteractive) != FALSE);
  QVERIFY(effectiveTokenIsInteractive != FALSE);

  BOOL directPrimaryResult = FALSE;
  SetLastError(ERROR_SUCCESS);
  QVERIFY(CheckTokenMembership(primaryToken.get(), const_cast<BYTE *>(sid.data()), &directPrimaryResult) == FALSE);
  const auto directPrimaryError = GetLastError();
  QCOMPARE(directPrimaryResult, FALSE);

  QVERIFY(deskflow::core::ipc::detail::hasInteractiveSidMembership(reinterpret_cast<quintptr>(primaryToken.get())));

  HANDLE queryOnlyTokenRaw = nullptr;
  QVERIFY(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &queryOnlyTokenRaw) != FALSE);
  const UniqueHandle queryOnlyToken(queryOnlyTokenRaw);
  QVERIFY(!deskflow::core::ipc::detail::hasInteractiveSidMembership(reinterpret_cast<quintptr>(queryOnlyToken.get())));

  qInfo(
      "direct primary-token membership failed with Win32 error %lu; duplicated-token membership succeeded",
      directPrimaryError
  );
}

void CoreIpcClientValidatorTests::obtainsRealClientPidFromServerSideLocalSocket()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  QLocalServer server;
  server.setSocketOptions(QLocalServer::WorldAccessOption);
  QVERIFY(server.listen(socketName));

  QLocalSocket client;
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));
  QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1000);

  auto *serverSocket = server.nextPendingConnection();
  QVERIFY(serverSocket != nullptr);
  const auto clientProcessId = deskflow::core::ipc::detail::namedPipeClientProcessId(*serverSocket);
  QVERIFY(clientProcessId.has_value());
  QCOMPARE(*clientProcessId, static_cast<quint32>(GetCurrentProcessId()));

  // This executable is intentionally not the adjacent Ieum.exe, so the full
  // production validator must consume the real pipe identity and deny it.
  QVERIFY(!deskflow::core::ipc::CoreIpcClientValidator{}.isAuthorized(*serverSocket));

  QLocalSocket unconnectedSocket;
  QVERIFY(!deskflow::core::ipc::detail::namedPipeClientProcessId(unconnectedSocket).has_value());
}

QTEST_GUILESS_MAIN(CoreIpcClientValidatorTests)

#include "CoreIpcClientValidatorTests.moc"
