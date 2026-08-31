/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/win32/MSWindowsFileTransferService.h"

#include "base/Log.h"
#include "platform/MSWindowsFileTransferSource.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace deskflow::filetransfer;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

class TemporaryDirectory final
{
public:
  TemporaryDirectory()
  {
    std::wstring buffer(MAX_PATH, L'\0');
    const auto length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (length == 0 || length >= buffer.size()) {
      throw std::runtime_error("GetTempPathW failed");
    }
    buffer.resize(length);

    std::wstring unique(MAX_PATH, L'\0');
    if (GetTempFileNameW(buffer.c_str(), L"ium", 0, unique.data()) == 0) {
      throw std::runtime_error("GetTempFileNameW failed");
    }
    unique.resize(std::char_traits<wchar_t>::length(unique.c_str()));
    if (DeleteFileW(unique.c_str()) == 0 || CreateDirectoryW(unique.c_str(), nullptr) == 0) {
      throw std::runtime_error("temporary directory creation failed");
    }
    m_path = std::filesystem::path(std::move(unique));
  }

  ~TemporaryDirectory()
  {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
  }

  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

  [[nodiscard]] const std::filesystem::path &path() const noexcept
  {
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

class FileHandle final
{
public:
  explicit FileHandle(HANDLE value) : m_value(value)
  {
  }

  ~FileHandle()
  {
    if (m_value != INVALID_HANDLE_VALUE) {
      CloseHandle(m_value);
    }
  }

  FileHandle(const FileHandle &) = delete;
  FileHandle &operator=(const FileHandle &) = delete;

  [[nodiscard]] HANDLE get() const noexcept
  {
    return m_value;
  }

private:
  HANDLE m_value = INVALID_HANDLE_VALUE;
};

void writeFile(const std::filesystem::path &path, std::string_view bytes)
{
  FileHandle file(
      CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)
  );
  if (file.get() == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("CreateFileW failed");
  }

  DWORD written = 0;
  if (!bytes.empty() &&
      (WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) == 0 ||
       written != bytes.size())) {
    throw std::runtime_error("WriteFile failed");
  }
}

std::string readFile(const std::filesystem::path &path)
{
  FileHandle file(
      CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)
  );
  if (file.get() == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("open received file failed");
  }

  LARGE_INTEGER size{};
  if (GetFileSizeEx(file.get(), &size) == 0 || size.QuadPart < 0) {
    throw std::runtime_error("GetFileSizeEx failed");
  }
  std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
  DWORD read = 0;
  if (!bytes.empty() && (ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) == 0 ||
                         read != bytes.size())) {
    throw std::runtime_error("ReadFile failed");
  }
  return bytes;
}

TransferId fixedId()
{
  TransferId id{};
  for (std::size_t index = 0; index < id.size(); ++index) {
    id[index] = static_cast<std::uint8_t>(index + 1);
  }
  return id;
}

std::string payload()
{
  std::string bytes(130'123, '\0');
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<char>('A' + (index % 23));
  }
  return bytes;
}

void transfersFileEndToEnd()
{
  TemporaryDirectory temporary;
  const auto sourcePath = temporary.path() / L"한글-alpha.bin";
  const auto destination = temporary.path() / L"received";
  const auto expected = payload();
  writeFile(sourcePath, expected);

  const auto inspected = inspectMSWindowsFileTransferSources({sourcePath});
  require(inspected.ok(), "source snapshot should succeed");

  std::vector<FileTransferControlMessage> senderControl;
  std::vector<FileTransferControlMessage> receiverControl;
  std::vector<FileTransferDataMessage> senderData;

  MSWindowsFileTransferService sender({
      .localScreen = "office",
      .destinationDirectory = {},
      .receiveEnabled = false,
      .authorizeFileTransfer = [] { return true; },
      .sendControl =
          [&](const FileTransferControlMessage &message) {
            senderControl.push_back(message);
            return true;
          },
      .sendData =
          [&](const FileTransferDataMessage &message) {
            senderData.push_back(message);
            return true;
          },
      .createTransferId = [] { return fixedId(); },
  });
  MSWindowsFileTransferService receiver({
      .localScreen = "laptop",
      .destinationDirectory = destination,
      .receiveEnabled = true,
      .authorizeFileTransfer = [] { return true; },
      .sendControl =
          [&](const FileTransferControlMessage &message) {
            receiverControl.push_back(message);
            return true;
          },
      .sendData = [](const FileTransferDataMessage &) { return false; },
  });

  require(sender.offerLocalFiles("laptop", inspected.sources), "local drop should create an offer");
  require(senderControl.size() == 1, "sender should emit one offer");
  require(receiver.handleControl(senderControl.front()), "receiver should accept a valid offer");
  require(receiverControl.size() == 1, "receiver should emit one decision");
  require(sender.handleControl(receiverControl.front()), "sender should apply the acceptance");

  std::size_t deliveredData = 0;
  for (std::size_t guard = 0; guard < 16; ++guard) {
    while (deliveredData < senderData.size()) {
      require(receiver.handleData(senderData[deliveredData++]), "receiver should apply ordered data");
    }
    if (!sender.processNextOutgoingChunk()) {
      break;
    }
  }
  while (deliveredData < senderData.size()) {
    require(receiver.handleData(senderData[deliveredData++]), "receiver should apply final data");
  }

  require(receiverControl.size() == 2, "receiver should emit completion after publishing");
  const auto *result = std::get_if<FileTransferResult>(&receiverControl.back());
  require(result != nullptr, "last receiver control should be a result");
  require(result->status == FileTransferResultStatus::Completed, "receiver should report completion");
  require(sender.handleControl(receiverControl.back()), "sender should apply completion");
  require(sender.activeSessionCount() == 0, "sender should release completed source metadata");
  require(receiver.activeSessionCount() == 0, "receiver should release completed staging state");
  require(readFile(destination / sourcePath.filename()) == expected, "published file should match the source bytes");
}

void rejectsWhenReceivingIsDisabled()
{
  TemporaryDirectory temporary;
  const auto sourcePath = temporary.path() / L"private.txt";
  const auto destination = temporary.path() / L"disabled";
  writeFile(sourcePath, "not accepted");
  const auto inspected = inspectMSWindowsFileTransferSources({sourcePath});
  require(inspected.ok(), "disabled test source snapshot should succeed");
  const auto manifest = buildFileTransferSourceManifest(fixedId(), "office", "laptop", inspected.sources);
  require(manifest.ok(), "disabled test manifest should be valid");

  std::vector<FileTransferControlMessage> controls;
  MSWindowsFileTransferService receiver({
      .localScreen = "laptop",
      .destinationDirectory = destination,
      .receiveEnabled = false,
      .authorizeFileTransfer = [] { return true; },
      .sendControl =
          [&](const FileTransferControlMessage &message) {
            controls.push_back(message);
            return true;
          },
      .sendData = [](const FileTransferDataMessage &) { return false; },
  });

  require(
      receiver.handleControl(FileTransferControlMessage{manifest.manifest.offer}), "disabled receiver should reject"
  );
  require(controls.size() == 1, "disabled receiver should emit one rejection");
  const auto *decision = std::get_if<FileTransferDecision>(&controls.front());
  require(decision != nullptr, "disabled receiver response should be a decision");
  require(decision->decision == FileTransferDecisionValue::Reject, "receiving must be opt-in");
  require(receiver.activeSessionCount() == 0, "rejected session should be erased");
  require(!std::filesystem::exists(destination), "disabled receiver should not create a destination directory");
}

void rejectsUnauthorizedOffers()
{
  TemporaryDirectory temporary;
  const auto sourcePath = temporary.path() / L"locked.txt";
  writeFile(sourcePath, "locked");
  const auto inspected = inspectMSWindowsFileTransferSources({sourcePath});
  require(inspected.ok(), "unauthorized source snapshot should succeed");

  bool authorized = false;
  std::vector<FileTransferControlMessage> controls;
  std::vector<FileTransferDataMessage> data;
  MSWindowsFileTransferService sender({
      .localScreen = "office",
      .destinationDirectory = {},
      .receiveEnabled = false,
      .authorizeFileTransfer = [&] { return authorized; },
      .sendControl =
          [&](const FileTransferControlMessage &message) {
            controls.push_back(message);
            return true;
          },
      .sendData =
          [&](const FileTransferDataMessage &message) {
            data.push_back(message);
            return true;
          },
      .createTransferId = [] { return fixedId(); },
  });

  require(!sender.offerLocalFiles("laptop", inspected.sources), "unauthorized sender must not create an offer");
  require(controls.empty(), "unauthorized sender must emit no control frame");
  require(data.empty(), "unauthorized sender must emit no data frame");
  require(sender.activeSessionCount() == 0, "unauthorized sender must create no session");

  authorized = true;
  require(sender.offerLocalFiles("laptop", inspected.sources), "authorization should permit a new offer");
  require(controls.size() == 1, "authorized sender should emit exactly one offer");
}

void rejectsUnauthorizedIncomingOffers()
{
  TemporaryDirectory temporary;
  const auto destination = temporary.path() / L"locked-receiver";
  const FileTransferOffer offer{
      .id = fixedId(),
      .sourceScreen = "office",
      .targetScreen = "laptop",
      .items = {{.index = 0, .name = "locked.txt", .size = 6}},
  };

  std::vector<FileTransferControlMessage> controls;
  MSWindowsFileTransferService receiver({
      .localScreen = "laptop",
      .destinationDirectory = destination,
      .receiveEnabled = true,
      .authorizeFileTransfer = [] { return false; },
      .sendControl =
          [&](const FileTransferControlMessage &message) {
            controls.push_back(message);
            return true;
          },
      .sendData = [](const FileTransferDataMessage &) { return false; },
  });

  require(!receiver.handleControl(FileTransferControlMessage{offer}), "unauthorized incoming offer must be denied");
  require(controls.empty(), "unauthorized receiver must emit no transfer control frame");
  require(receiver.activeSessionCount() == 0, "unauthorized receiver must create no session");
  require(!std::filesystem::exists(destination), "unauthorized receiver must not create its destination");
}

void stopsOutgoingTransferWhenAuthorizationIsRevoked()
{
  TemporaryDirectory temporary;
  const auto sourcePath = temporary.path() / L"revoked.bin";
  writeFile(sourcePath, payload());
  const auto inspected = inspectMSWindowsFileTransferSources({sourcePath});
  require(inspected.ok(), "revocation source snapshot should succeed");

  bool authorized = true;
  std::vector<FileTransferControlMessage> controls;
  std::vector<FileTransferDataMessage> data;
  MSWindowsFileTransferService sender({
      .localScreen = "office",
      .destinationDirectory = {},
      .receiveEnabled = false,
      .authorizeFileTransfer = [&] { return authorized; },
      .sendControl =
          [&](const FileTransferControlMessage &message) {
            controls.push_back(message);
            return true;
          },
      .sendData =
          [&](const FileTransferDataMessage &message) {
            data.push_back(message);
            return true;
          },
      .createTransferId = [] { return fixedId(); },
  });

  require(sender.offerLocalFiles("laptop", inspected.sources), "authorized sender should offer the source");
  const auto *offer = std::get_if<FileTransferOffer>(&controls.front());
  require(offer != nullptr, "first control frame should be an offer");
  require(
      sender.handleControl(
          FileTransferControlMessage{FileTransferDecision{
              .id = offer->id,
              .sourceScreen = offer->sourceScreen,
              .targetScreen = offer->targetScreen,
              .decision = FileTransferDecisionValue::Accept,
          }}
      ),
      "authorized sender should begin after acceptance"
  );
  require(
      data.size() == 1 && std::holds_alternative<FileTransferDataBegin>(data.front()),
      "accepted sender should emit only the begin frame before its first chunk"
  );

  authorized = false;
  const auto dataBeforeRevocation = data.size();
  require(!sender.processNextOutgoingChunk(), "revoked sender must stop before its next chunk");
  require(data.size() == dataBeforeRevocation, "revoked sender must emit no additional data");
  require(sender.activeSessionCount() == 0, "revocation must erase the outgoing session");
  require(controls.size() == 2, "revocation should notify the peer once");
  const auto *result = std::get_if<FileTransferResult>(&controls.back());
  require(
      result != nullptr && result->status == FileTransferResultStatus::Failed,
      "revocation notification should be a failed result"
  );
  require(
      result->code == FileTransferResultCode::AuthorizationFailed,
      "revocation notification should identify authorization failure"
  );

  authorized = true;
  require(!sender.processNextOutgoingChunk(), "restoring authorization must not resurrect a failed session");
  require(data.size() == dataBeforeRevocation, "restored authorization must not replay old data");
}

void stopsIncomingTransferWhenAuthorizationIsRevoked()
{
  TemporaryDirectory temporary;
  const auto destination = temporary.path() / L"revoked-receiver";
  const FileTransferOffer offer{
      .id = fixedId(),
      .sourceScreen = "office",
      .targetScreen = "laptop",
      .items = {{.index = 0, .name = "revoked.txt", .size = 6}},
  };

  bool authorized = true;
  std::vector<FileTransferControlMessage> controls;
  MSWindowsFileTransferService receiver({
      .localScreen = "laptop",
      .destinationDirectory = destination,
      .receiveEnabled = true,
      .authorizeFileTransfer = [&] { return authorized; },
      .sendControl =
          [&](const FileTransferControlMessage &message) {
            controls.push_back(message);
            return true;
          },
      .sendData = [](const FileTransferDataMessage &) { return false; },
  });

  require(receiver.handleControl(FileTransferControlMessage{offer}), "authorized receiver should accept the offer");
  require(controls.size() == 1, "authorized receiver should emit one acceptance");

  authorized = false;
  const FileTransferDataRoute route{
      .id = offer.id,
      .sourceScreen = offer.sourceScreen,
      .targetScreen = offer.targetScreen,
  };
  require(
      !receiver.handleData(FileTransferDataMessage{FileTransferDataBegin{.route = route}}),
      "revoked receiver must reject incoming data"
  );
  require(receiver.activeSessionCount() == 0, "revocation must erase the incoming session");
  require(controls.size() == 2, "revoked receiver should notify the peer once");
  const auto *result = std::get_if<FileTransferResult>(&controls.back());
  require(
      result != nullptr && result->code == FileTransferResultCode::AuthorizationFailed,
      "incoming revocation should identify authorization failure"
  );
}

} // namespace

int main()
{
  Log log;
  transfersFileEndToEnd();
  rejectsWhenReceivingIsDisabled();
  rejectsUnauthorizedOffers();
  rejectsUnauthorizedIncomingOffers();
  stopsOutgoingTransferWhenAuthorizationIsRevoked();
  stopsIncomingTransferWhenAuthorizationIsRevoked();
  std::cout << "PASS: MSWindowsFileTransferServiceTests\n";
  return 0;
}
