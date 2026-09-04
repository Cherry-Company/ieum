/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/win32/CoreIpcClientValidator.h"
#include "deskflow/win32/CoreIpcClientValidatorDetail.h"

#include <QLocalSocket>
#include <QtTypes>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WtsApi32.h>

#include <array>
#include <filesystem>
#include <limits>
#include <optional>
#include <system_error>
#include <vector>

namespace deskflow::core::ipc {
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

} // namespace

namespace detail {

bool hasInteractiveSidMembership(quintptr primaryToken) noexcept
{
  const auto token = reinterpret_cast<HANDLE>(primaryToken);
  if (token == nullptr || token == INVALID_HANDLE_VALUE) {
    return false;
  }

  HANDLE impersonationTokenRaw = nullptr;
  if (DuplicateToken(token, SecurityImpersonation, &impersonationTokenRaw) == FALSE) {
    return false;
  }
  const UniqueHandle impersonationToken(impersonationTokenRaw);

  std::array<BYTE, SECURITY_MAX_SID_SIZE> interactiveSid{};
  DWORD interactiveSidSize = static_cast<DWORD>(interactiveSid.size());
  if (CreateWellKnownSid(WinInteractiveSid, nullptr, interactiveSid.data(), &interactiveSidSize) == FALSE) {
    return false;
  }

  BOOL isInteractive = FALSE;
  return CheckTokenMembership(impersonationToken.get(), interactiveSid.data(), &isInteractive) != FALSE &&
         isInteractive != FALSE;
}

std::optional<quint32> namedPipeClientProcessId(const QLocalSocket &clientSocket) noexcept
{
  const auto descriptor = clientSocket.socketDescriptor();
  if (descriptor == -1) {
    return std::nullopt;
  }
  const auto pipe = reinterpret_cast<HANDLE>(static_cast<quintptr>(descriptor));
  if (pipe == nullptr || pipe == INVALID_HANDLE_VALUE) {
    return std::nullopt;
  }

  ULONG clientProcessId = 0;
  if (GetNamedPipeClientProcessId(pipe, &clientProcessId) == FALSE || clientProcessId == 0) {
    return std::nullopt;
  }
  return static_cast<quint32>(clientProcessId);
}

} // namespace detail

namespace {

bool tokenInformation(HANDLE token, TOKEN_INFORMATION_CLASS infoClass, std::vector<BYTE> &buffer)
{
  DWORD required = 0;
  if (GetTokenInformation(token, infoClass, nullptr, 0, &required) != FALSE ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER || required == 0) {
    return false;
  }

  buffer.resize(required);
  DWORD received = 0;
  return GetTokenInformation(token, infoClass, buffer.data(), required, &received) != FALSE && received <= required;
}

PSID tokenUserSid(HANDLE token, std::vector<BYTE> &buffer)
{
  if (!tokenInformation(token, TokenUser, buffer) || buffer.size() < sizeof(TOKEN_USER)) {
    return nullptr;
  }

  const auto user = reinterpret_cast<const TOKEN_USER *>(buffer.data());
  return IsValidSid(user->User.Sid) != FALSE ? user->User.Sid : nullptr;
}

bool hasExpectedSession(HANDLE token, DWORD expectedSession)
{
  DWORD tokenSession = 0;
  DWORD received = 0;
  return GetTokenInformation(token, TokenSessionId, &tokenSession, sizeof(tokenSession), &received) != FALSE &&
         received == sizeof(tokenSession) && tokenSession == expectedSession;
}

bool isInteractiveToken(HANDLE token, DWORD sessionId)
{
  if (!detail::hasInteractiveSidMembership(reinterpret_cast<quintptr>(token))) {
    return false;
  }

  HANDLE sessionTokenRaw = nullptr;
  if (WTSQueryUserToken(sessionId, &sessionTokenRaw) == FALSE) {
    return false;
  }
  const UniqueHandle sessionToken(sessionTokenRaw);

  std::vector<BYTE> clientUserBuffer;
  std::vector<BYTE> sessionUserBuffer;
  const auto clientUser = tokenUserSid(token, clientUserBuffer);
  const auto sessionUser = tokenUserSid(sessionToken.get(), sessionUserBuffer);
  return clientUser != nullptr && sessionUser != nullptr && EqualSid(clientUser, sessionUser) != FALSE;
}

bool isPermittedUserToken(HANDLE token, DWORD sessionId)
{
  std::vector<BYTE> userBuffer;
  const auto userSid = tokenUserSid(token, userBuffer);
  if (userSid == nullptr || IsWellKnownSid(userSid, WinLocalSystemSid) != FALSE) {
    return false;
  }

  DWORD isAppContainer = 0;
  DWORD received = 0;
  if (GetTokenInformation(
          token, TokenIsAppContainer, &isAppContainer, sizeof(isAppContainer), &received
      ) == FALSE ||
      received != sizeof(isAppContainer) || isAppContainer != 0) {
    return false;
  }

  std::vector<BYTE> integrityBuffer;
  if (!tokenInformation(token, TokenIntegrityLevel, integrityBuffer) ||
      integrityBuffer.size() < sizeof(TOKEN_MANDATORY_LABEL)) {
    return false;
  }
  const auto label = reinterpret_cast<const TOKEN_MANDATORY_LABEL *>(integrityBuffer.data());
  if (IsValidSid(label->Label.Sid) == FALSE) {
    return false;
  }
  const auto subAuthorityCount = GetSidSubAuthorityCount(label->Label.Sid);
  if (subAuthorityCount == nullptr || *subAuthorityCount == 0) {
    return false;
  }
  const auto integrityRid = GetSidSubAuthority(label->Label.Sid, *subAuthorityCount - 1);
  if (integrityRid == nullptr || *integrityRid < SECURITY_MANDATORY_MEDIUM_RID) {
    return false;
  }

  return hasExpectedSession(token, sessionId) && isInteractiveToken(token, sessionId);
}

std::optional<std::filesystem::path> processImagePath(HANDLE process)
{
  constexpr DWORD kPathCapacity = 32 * 1024;
  std::vector<wchar_t> buffer(kPathCapacity);
  DWORD length = static_cast<DWORD>(buffer.size());
  if (QueryFullProcessImageNameW(process, 0, buffer.data(), &length) == FALSE || length == 0 ||
      length >= buffer.size()) {
    return std::nullopt;
  }

  const std::filesystem::path rawPath(std::wstring_view(buffer.data(), length));
  if (!rawPath.is_absolute()) {
    return std::nullopt;
  }

  std::error_code error;
  auto normalized = std::filesystem::canonical(rawPath, error);
  if (error || !normalized.is_absolute()) {
    return std::nullopt;
  }
  return normalized.lexically_normal();
}

bool samePathIgnoringCase(const std::filesystem::path &left, const std::filesystem::path &right)
{
  const auto &leftNative = left.native();
  const auto &rightNative = right.native();
  if (leftNative.size() > (std::numeric_limits<int>::max)() ||
      rightNative.size() > (std::numeric_limits<int>::max)()) {
    return false;
  }

  return CompareStringOrdinal(
             leftNative.data(), static_cast<int>(leftNative.size()), rightNative.data(),
             static_cast<int>(rightNative.size()), TRUE
         ) == CSTR_EQUAL;
}

bool hasExpectedImagePath(HANDLE clientProcess)
{
  const auto clientPath = processImagePath(clientProcess);
  const auto corePath = processImagePath(GetCurrentProcess());
  if (!clientPath.has_value() || !corePath.has_value() || !corePath->has_parent_path()) {
    return false;
  }

  std::error_code error;
  auto expectedPath = std::filesystem::canonical(corePath->parent_path() / L"Ieum.exe", error);
  if (error || !expectedPath.is_absolute()) {
    return false;
  }
  expectedPath = expectedPath.lexically_normal();
  return samePathIgnoringCase(*clientPath, expectedPath);
}

bool validateClient(const QLocalSocket &clientSocket)
{
  const auto clientProcessId = detail::namedPipeClientProcessId(clientSocket);
  if (!clientProcessId.has_value()) {
    return false;
  }

  const UniqueHandle clientProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, *clientProcessId));
  if (clientProcess.get() == nullptr) {
    return false;
  }

  DWORD coreSession = 0;
  DWORD clientSession = 0;
  if (ProcessIdToSessionId(GetCurrentProcessId(), &coreSession) == FALSE ||
      ProcessIdToSessionId(*clientProcessId, &clientSession) == FALSE || coreSession != clientSession) {
    return false;
  }

  if (!hasExpectedImagePath(clientProcess.get())) {
    return false;
  }

  HANDLE clientTokenRaw = nullptr;
  if (OpenProcessToken(clientProcess.get(), TOKEN_QUERY | TOKEN_DUPLICATE, &clientTokenRaw) == FALSE) {
    return false;
  }
  const UniqueHandle clientToken(clientTokenRaw);
  if (!isPermittedUserToken(clientToken.get(), clientSession)) {
    return false;
  }

  DWORD exitCode = 0;
  const auto confirmedClientProcessId = detail::namedPipeClientProcessId(clientSocket);
  return GetExitCodeProcess(clientProcess.get(), &exitCode) != FALSE && exitCode == STILL_ACTIVE &&
         confirmedClientProcessId.has_value() && *confirmedClientProcessId == *clientProcessId;
}

} // namespace

bool CoreIpcClientValidator::isAuthorized(const QLocalSocket &clientSocket) const noexcept
{
  try {
    return validateClient(clientSocket);
  } catch (...) {
    return false;
  }
}

} // namespace deskflow::core::ipc
