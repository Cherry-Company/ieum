/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsFileTransferIo.h"

#include "deskflow/FileTransferOfferValidator.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace deskflow::filetransfer {
namespace {

constexpr std::size_t kMaximumChunkBytes = 60 * 1024;
constexpr std::uint32_t kMaximumPublishAttempts = 10'000;

bool ntSuccess(NTSTATUS status) noexcept
{
  return status >= 0;
}

class FileHandle final
{
public:
  FileHandle() = default;
  explicit FileHandle(HANDLE handle) noexcept : m_handle(handle)
  {
  }

  ~FileHandle()
  {
    reset();
  }

  FileHandle(const FileHandle &) = delete;
  FileHandle &operator=(const FileHandle &) = delete;

  FileHandle(FileHandle &&other) noexcept : m_handle(std::exchange(other.m_handle, INVALID_HANDLE_VALUE))
  {
  }

  FileHandle &operator=(FileHandle &&other) noexcept
  {
    if (this != &other) {
      reset();
      m_handle = std::exchange(other.m_handle, INVALID_HANDLE_VALUE);
    }
    return *this;
  }

  [[nodiscard]] HANDLE get() const noexcept
  {
    return m_handle;
  }

  [[nodiscard]] bool valid() const noexcept
  {
    return m_handle != INVALID_HANDLE_VALUE;
  }

  void reset(HANDLE handle = INVALID_HANDLE_VALUE) noexcept
  {
    if (valid()) {
      CloseHandle(m_handle);
    }
    m_handle = handle;
  }

private:
  HANDLE m_handle = INVALID_HANDLE_VALUE;
};

class Sha256 final
{
public:
  Sha256()
  {
    if (!ntSuccess(BCryptOpenAlgorithmProvider(&m_algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
      return;
    }

    DWORD objectBytes = 0;
    DWORD copied = 0;
    if (!ntSuccess(BCryptGetProperty(
            m_algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &copied, 0
        )) ||
        copied != sizeof(objectBytes) || objectBytes == 0) {
      return;
    }

    DWORD digestBytes = 0;
    if (!ntSuccess(BCryptGetProperty(
            m_algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&digestBytes), sizeof(digestBytes), &copied, 0
        )) ||
        copied != sizeof(digestBytes) || digestBytes != FileTransferDigest{}.size()) {
      return;
    }

    m_object.resize(objectBytes);
    if (!ntSuccess(
            BCryptCreateHash(m_algorithm, &m_hash, m_object.data(), static_cast<ULONG>(m_object.size()), nullptr, 0, 0)
        )) {
      m_hash = nullptr;
      return;
    }
    m_valid = true;
  }

  ~Sha256()
  {
    if (m_hash != nullptr) {
      BCryptDestroyHash(m_hash);
    }
    if (m_algorithm != nullptr) {
      BCryptCloseAlgorithmProvider(m_algorithm, 0);
    }
  }

  Sha256(const Sha256 &) = delete;
  Sha256 &operator=(const Sha256 &) = delete;

  [[nodiscard]] bool valid() const noexcept
  {
    return m_valid && !m_finished;
  }

  bool update(std::span<const std::uint8_t> bytes)
  {
    if (!valid() || bytes.size() > (std::numeric_limits<ULONG>::max)()) {
      return false;
    }
    if (bytes.empty()) {
      return true;
    }
    return ntSuccess(BCryptHashData(
        m_hash, const_cast<PUCHAR>(reinterpret_cast<const UCHAR *>(bytes.data())), static_cast<ULONG>(bytes.size()), 0
    ));
  }

  std::optional<FileTransferDigest> finish()
  {
    if (!valid()) {
      return std::nullopt;
    }

    FileTransferDigest digest{};
    if (!ntSuccess(BCryptFinishHash(m_hash, digest.data(), static_cast<ULONG>(digest.size()), 0))) {
      return std::nullopt;
    }
    m_finished = true;
    return digest;
  }

private:
  BCRYPT_ALG_HANDLE m_algorithm = nullptr;
  BCRYPT_HASH_HANDLE m_hash = nullptr;
  std::vector<std::uint8_t> m_object;
  bool m_valid = false;
  bool m_finished = false;
};

std::uint64_t combinedValue(DWORD high, DWORD low) noexcept
{
  return (static_cast<std::uint64_t>(high) << 32U) | low;
}

FileTransferSourceIdentity fileIdentity(const BY_HANDLE_FILE_INFORMATION &information)
{
  FileTransferSourceIdentity identity;
  constexpr auto volumeOffset = std::size_t{0};
  constexpr auto indexHighOffset = sizeof(information.dwVolumeSerialNumber);
  constexpr auto indexLowOffset = indexHighOffset + sizeof(information.nFileIndexHigh);
  identity.length = indexLowOffset + sizeof(information.nFileIndexLow);
  std::memcpy(
      identity.bytes.data() + volumeOffset, &information.dwVolumeSerialNumber, sizeof(information.dwVolumeSerialNumber)
  );
  std::memcpy(identity.bytes.data() + indexHighOffset, &information.nFileIndexHigh, sizeof(information.nFileIndexHigh));
  std::memcpy(identity.bytes.data() + indexLowOffset, &information.nFileIndexLow, sizeof(information.nFileIndexLow));
  return identity;
}

bool matchesSource(HANDLE handle, const FileTransferSourceCandidate &source)
{
  BY_HANDLE_FILE_INFORMATION information{};
  if (GetFileInformationByHandle(handle, &information) == 0 ||
      (information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0 ||
      GetFileType(handle) != FILE_TYPE_DISK) {
    return false;
  }

  return fileIdentity(information) == source.identity &&
         combinedValue(information.nFileSizeHigh, information.nFileSizeLow) == source.size &&
         combinedValue(information.ftLastWriteTime.dwHighDateTime, information.ftLastWriteTime.dwLowDateTime) ==
             source.lastWriteTime;
}

bool validSource(const FileTransferSourceCandidate &source, std::size_t chunkBytes)
{
  return !source.path.empty() && source.path.is_absolute() && source.identity.length > 0 &&
         source.identity.length <= source.identity.bytes.size() && chunkBytes > 0 && chunkBytes <= kMaximumChunkBytes;
}

std::optional<std::wstring> utf8ToWide(const std::string &value)
{
  if (value.empty() || value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
    return std::nullopt;
  }
  const auto inputLength = static_cast<int>(value.size());
  const auto outputLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), inputLength, nullptr, 0);
  if (outputLength <= 0) {
    return std::nullopt;
  }

  std::wstring result(static_cast<std::size_t>(outputLength), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), inputLength, result.data(), outputLength) !=
      outputLength) {
    return std::nullopt;
  }
  return result;
}

std::wstring transferIdHex(const TransferId &id)
{
  constexpr std::wstring_view digits = L"0123456789abcdef";
  std::wstring result;
  result.reserve(id.size() * 2);
  for (const auto byte : id) {
    result.push_back(digits[byte >> 4U]);
    result.push_back(digits[byte & 0x0fU]);
  }
  return result;
}

bool routeMatches(const FileTransferOffer &offer, const FileTransferDataRoute &route)
{
  return offer.id == route.id && offer.sourceScreen == route.sourceScreen && offer.targetScreen == route.targetScreen;
}

MSWindowsFileTransferWriterMutation mutationFailure(MSWindowsFileTransferIoError error)
{
  return {.error = error, .publishedPaths = {}};
}

} // namespace

struct MSWindowsFileTransferReader::Impl
{
  FileTransferSourceCandidate source;
  std::size_t chunkBytes = 0;
  FileHandle file;
  Sha256 hash;
  std::uint64_t offset = 0;
  bool complete = false;
  bool failed = false;
};

MSWindowsFileTransferReader::MSWindowsFileTransferReader(std::unique_ptr<Impl> impl) : m_impl(std::move(impl))
{
}

MSWindowsFileTransferReader::~MSWindowsFileTransferReader() = default;

MSWindowsFileTransferReaderOpenResult
MSWindowsFileTransferReader::open(FileTransferSourceCandidate source, std::size_t chunkBytes)
{
  if (!validSource(source, chunkBytes)) {
    return {.error = MSWindowsFileTransferIoError::InvalidSource, .reader = nullptr};
  }

  FileHandle file(CreateFileW(
      source.path.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr
  ));
  if (!file.valid()) {
    return {.error = MSWindowsFileTransferIoError::OpenFailed, .reader = nullptr};
  }
  if (!matchesSource(file.get(), source)) {
    return {.error = MSWindowsFileTransferIoError::SourceChanged, .reader = nullptr};
  }

  auto impl = std::make_unique<Impl>();
  if (!impl->hash.valid()) {
    return {.error = MSWindowsFileTransferIoError::HashFailed, .reader = nullptr};
  }
  impl->source = std::move(source);
  impl->chunkBytes = chunkBytes;
  impl->file = std::move(file);
  return {
      .error = MSWindowsFileTransferIoError::None,
      .reader = std::unique_ptr<MSWindowsFileTransferReader>(new MSWindowsFileTransferReader(std::move(impl))),
  };
}

MSWindowsFileTransferReadResult MSWindowsFileTransferReader::readNext()
{
  if (m_impl->complete || m_impl->failed) {
    return {.error = MSWindowsFileTransferIoError::WrongPhase};
  }

  MSWindowsFileTransferReadResult result;
  result.offset = m_impl->offset;
  const auto remaining = m_impl->source.size - m_impl->offset;
  const auto requested =
      static_cast<std::size_t>((std::min)(remaining, static_cast<std::uint64_t>(m_impl->chunkBytes)));
  result.bytes.resize(requested);

  std::size_t received = 0;
  while (received < requested) {
    DWORD count = 0;
    const auto next = static_cast<DWORD>((std::min)(requested - received,
                                                    static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
    if (ReadFile(m_impl->file.get(), result.bytes.data() + received, next, &count, nullptr) == 0) {
      m_impl->failed = true;
      return {.error = MSWindowsFileTransferIoError::ReadFailed};
    }
    if (count == 0) {
      m_impl->failed = true;
      return {.error = MSWindowsFileTransferIoError::SourceChanged};
    }
    received += count;
  }

  if (!m_impl->hash.update(result.bytes)) {
    m_impl->failed = true;
    return {.error = MSWindowsFileTransferIoError::HashFailed};
  }
  m_impl->offset += result.bytes.size();
  if (m_impl->offset < m_impl->source.size) {
    return result;
  }

  if (!matchesSource(m_impl->file.get(), m_impl->source)) {
    m_impl->failed = true;
    return {.error = MSWindowsFileTransferIoError::SourceChanged};
  }
  const auto digest = m_impl->hash.finish();
  if (!digest.has_value()) {
    m_impl->failed = true;
    return {.error = MSWindowsFileTransferIoError::HashFailed};
  }

  m_impl->complete = true;
  result.complete = true;
  result.sha256 = *digest;
  return result;
}

struct MSWindowsFileTransferWriter::Impl
{
  enum class State
  {
    AwaitingBegin,
    Receiving,
    ReadyToFinish,
    Completed,
    Failed,
  };

  std::filesystem::path destination;
  std::filesystem::path staging;
  FileTransferOffer offer;
  std::vector<std::wstring> itemNames;
  std::uint64_t expectedTotalBytes = 0;
  std::uint64_t receivedTotalBytes = 0;
  std::uint64_t currentOffset = 0;
  std::size_t currentItem = 0;
  FileHandle currentFile;
  std::unique_ptr<Sha256> currentHash;
  std::vector<std::filesystem::path> published;
  State state = State::AwaitingBegin;
  bool ownsStaging = false;

  [[nodiscard]] std::filesystem::path stagedPath(std::size_t item) const
  {
    return staging / (std::to_wstring(item) + L".part");
  }

  void cleanupStaging() noexcept
  {
    currentHash.reset();
    currentFile.reset();
    if (!ownsStaging) {
      return;
    }
    for (std::size_t item = 0; item < offer.items.size(); ++item) {
      DeleteFileW(stagedPath(item).c_str());
    }
    if (RemoveDirectoryW(staging.c_str()) != 0) {
      ownsStaging = false;
    }
  }

  MSWindowsFileTransferWriterMutation fail(MSWindowsFileTransferIoError error)
  {
    state = State::Failed;
    cleanupStaging();
    return mutationFailure(error);
  }

  bool ensureCurrentFile()
  {
    if (currentFile.valid()) {
      return true;
    }

    currentFile.reset(CreateFileW(
        stagedPath(currentItem).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr
    ));
    if (!currentFile.valid()) {
      return false;
    }
    currentHash = std::make_unique<Sha256>();
    return currentHash->valid();
  }

  std::optional<std::filesystem::path> publishItem(std::size_t item)
  {
    const std::filesystem::path filename(itemNames[item]);
    const auto stem = filename.stem().native();
    const auto extension = filename.extension().native();
    for (std::uint32_t attempt = 0; attempt < kMaximumPublishAttempts; ++attempt) {
      auto candidateName = filename.native();
      if (attempt > 0) {
        candidateName = stem + L" (" + std::to_wstring(attempt) + L")" + extension;
      }
      const auto candidate = destination / candidateName;
      if (MoveFileExW(stagedPath(item).c_str(), candidate.c_str(), MOVEFILE_WRITE_THROUGH) != 0) {
        return candidate;
      }
      const auto error = GetLastError();
      if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
        return std::nullopt;
      }
    }
    return std::nullopt;
  }
};

MSWindowsFileTransferWriter::MSWindowsFileTransferWriter(std::unique_ptr<Impl> impl) : m_impl(std::move(impl))
{
}

MSWindowsFileTransferWriter::~MSWindowsFileTransferWriter()
{
  m_impl->cleanupStaging();
}

MSWindowsFileTransferWriterCreateResult
MSWindowsFileTransferWriter::create(std::filesystem::path destinationDirectory, FileTransferOffer offer)
{
  const auto validation = validateOffer(offer);
  if (!validation.ok()) {
    return {.error = MSWindowsFileTransferIoError::InvalidOffer, .writer = nullptr};
  }
  if (destinationDirectory.empty() || !destinationDirectory.is_absolute()) {
    return {.error = MSWindowsFileTransferIoError::InvalidDestination, .writer = nullptr};
  }

  const auto attributes = GetFileAttributesW(destinationDirectory.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
      (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    return {.error = MSWindowsFileTransferIoError::InvalidDestination, .writer = nullptr};
  }

  std::vector<std::wstring> itemNames;
  itemNames.reserve(offer.items.size());
  for (const auto &item : offer.items) {
    auto name = utf8ToWide(item.name);
    if (!name.has_value()) {
      return {.error = MSWindowsFileTransferIoError::InvalidOffer, .writer = nullptr};
    }
    itemNames.push_back(std::move(*name));
  }

  auto impl = std::make_unique<Impl>();
  impl->destination = std::move(destinationDirectory);
  impl->staging = impl->destination / (L".ieum-" + transferIdHex(offer.id) + L".part");
  impl->offer = std::move(offer);
  impl->itemNames = std::move(itemNames);
  impl->expectedTotalBytes = validation.totalBytes;
  return {
      .error = MSWindowsFileTransferIoError::None,
      .writer = std::unique_ptr<MSWindowsFileTransferWriter>(new MSWindowsFileTransferWriter(std::move(impl))),
  };
}

MSWindowsFileTransferWriterMutation MSWindowsFileTransferWriter::begin(const FileTransferDataBegin &message)
{
  if (m_impl->state != Impl::State::AwaitingBegin) {
    return mutationFailure(MSWindowsFileTransferIoError::WrongPhase);
  }
  if (!routeMatches(m_impl->offer, message.route)) {
    return m_impl->fail(MSWindowsFileTransferIoError::RouteMismatch);
  }
  if (CreateDirectoryW(m_impl->staging.c_str(), nullptr) == 0) {
    return m_impl->fail(MSWindowsFileTransferIoError::DestinationDenied);
  }
  m_impl->ownsStaging = true;
  SetFileAttributesW(m_impl->staging.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY);
  m_impl->state = Impl::State::Receiving;
  return {};
}

MSWindowsFileTransferWriterMutation MSWindowsFileTransferWriter::writeChunk(const FileTransferDataChunk &message)
{
  if (m_impl->state != Impl::State::Receiving) {
    return mutationFailure(MSWindowsFileTransferIoError::WrongPhase);
  }
  if (!routeMatches(m_impl->offer, message.route)) {
    return m_impl->fail(MSWindowsFileTransferIoError::RouteMismatch);
  }
  if (message.itemIndex != m_impl->currentItem) {
    return m_impl->fail(MSWindowsFileTransferIoError::WrongItem);
  }
  if (message.offset != m_impl->currentOffset) {
    return m_impl->fail(MSWindowsFileTransferIoError::WrongOffset);
  }
  if (message.bytes.empty() || message.bytes.size() > kMaximumChunkBytes ||
      message.bytes.size() > std::numeric_limits<std::uint64_t>::max() - m_impl->currentOffset ||
      m_impl->currentOffset + message.bytes.size() > m_impl->offer.items[m_impl->currentItem].size) {
    return m_impl->fail(MSWindowsFileTransferIoError::SizeExceeded);
  }
  if (!m_impl->ensureCurrentFile()) {
    return m_impl->fail(MSWindowsFileTransferIoError::DestinationDenied);
  }

  std::size_t written = 0;
  while (written < message.bytes.size()) {
    DWORD count = 0;
    const auto next = static_cast<DWORD>((std::min)(message.bytes.size() - written,
                                                    static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
    if (WriteFile(m_impl->currentFile.get(), message.bytes.data() + written, next, &count, nullptr) == 0 ||
        count == 0) {
      return m_impl->fail(MSWindowsFileTransferIoError::WriteFailed);
    }
    written += count;
  }
  if (!m_impl->currentHash->update(message.bytes)) {
    return m_impl->fail(MSWindowsFileTransferIoError::HashFailed);
  }
  m_impl->currentOffset += message.bytes.size();
  m_impl->receivedTotalBytes += message.bytes.size();
  return {};
}

MSWindowsFileTransferWriterMutation MSWindowsFileTransferWriter::endItem(const FileTransferDataItemEnd &message)
{
  if (m_impl->state != Impl::State::Receiving) {
    return mutationFailure(MSWindowsFileTransferIoError::WrongPhase);
  }
  if (!routeMatches(m_impl->offer, message.route)) {
    return m_impl->fail(MSWindowsFileTransferIoError::RouteMismatch);
  }
  if (message.itemIndex != m_impl->currentItem) {
    return m_impl->fail(MSWindowsFileTransferIoError::WrongItem);
  }

  const auto expectedSize = m_impl->offer.items[m_impl->currentItem].size;
  if (message.size != expectedSize || m_impl->currentOffset != expectedSize) {
    return m_impl->fail(MSWindowsFileTransferIoError::SizeExceeded);
  }
  if (!m_impl->ensureCurrentFile()) {
    return m_impl->fail(MSWindowsFileTransferIoError::DestinationDenied);
  }

  const auto digest = m_impl->currentHash->finish();
  if (!digest.has_value()) {
    return m_impl->fail(MSWindowsFileTransferIoError::HashFailed);
  }
  if (*digest != message.sha256) {
    return m_impl->fail(MSWindowsFileTransferIoError::IntegrityMismatch);
  }
  if (FlushFileBuffers(m_impl->currentFile.get()) == 0) {
    return m_impl->fail(MSWindowsFileTransferIoError::WriteFailed);
  }

  m_impl->currentHash.reset();
  m_impl->currentFile.reset();
  ++m_impl->currentItem;
  m_impl->currentOffset = 0;
  m_impl->state =
      m_impl->currentItem == m_impl->offer.items.size() ? Impl::State::ReadyToFinish : Impl::State::Receiving;
  return {};
}

MSWindowsFileTransferWriterMutation MSWindowsFileTransferWriter::finish(const FileTransferDataFinish &message)
{
  if (m_impl->state != Impl::State::ReadyToFinish) {
    return mutationFailure(MSWindowsFileTransferIoError::WrongPhase);
  }
  if (!routeMatches(m_impl->offer, message.route)) {
    return m_impl->fail(MSWindowsFileTransferIoError::RouteMismatch);
  }
  if (message.itemCount != m_impl->offer.items.size() || message.totalBytes != m_impl->expectedTotalBytes ||
      m_impl->receivedTotalBytes != m_impl->expectedTotalBytes) {
    return m_impl->fail(MSWindowsFileTransferIoError::SizeExceeded);
  }

  for (std::size_t item = 0; item < m_impl->offer.items.size(); ++item) {
    const auto published = m_impl->publishItem(item);
    if (!published.has_value()) {
      for (const auto &path : m_impl->published) {
        DeleteFileW(path.c_str());
      }
      m_impl->published.clear();
      return m_impl->fail(MSWindowsFileTransferIoError::PublishFailed);
    }
    m_impl->published.push_back(*published);
  }

  if (RemoveDirectoryW(m_impl->staging.c_str()) != 0) {
    m_impl->ownsStaging = false;
  }
  m_impl->state = Impl::State::Completed;
  return {.error = MSWindowsFileTransferIoError::None, .publishedPaths = m_impl->published};
}

} // namespace deskflow::filetransfer
