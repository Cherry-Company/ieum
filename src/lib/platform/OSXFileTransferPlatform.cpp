/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXFileTransferPlatform.h"

#include "deskflow/FileTransferOfferValidator.h"

#include <QByteArray>
#include <QString>
#include <QStringDecoder>

#include <Security/SecRandom.h>
#include <openssl/evp.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>

namespace deskflow::filetransfer {
namespace {

constexpr std::size_t kMaximumChunkBytes = 60 * 1024;
constexpr std::uint32_t kMaximumPublishAttempts = 10'000;

class FileDescriptor final
{
public:
  FileDescriptor() = default;
  explicit FileDescriptor(int descriptor) noexcept : m_descriptor(descriptor)
  {
  }
  ~FileDescriptor()
  {
    reset();
  }
  FileDescriptor(const FileDescriptor &) = delete;
  FileDescriptor &operator=(const FileDescriptor &) = delete;
  FileDescriptor(FileDescriptor &&other) noexcept : m_descriptor(std::exchange(other.m_descriptor, -1))
  {
  }
  FileDescriptor &operator=(FileDescriptor &&other) noexcept
  {
    if (this != &other) {
      reset();
      m_descriptor = std::exchange(other.m_descriptor, -1);
    }
    return *this;
  }
  [[nodiscard]] int get() const noexcept
  {
    return m_descriptor;
  }
  [[nodiscard]] bool valid() const noexcept
  {
    return m_descriptor >= 0;
  }
  void reset(int descriptor = -1) noexcept
  {
    if (valid()) {
      ::close(m_descriptor);
    }
    m_descriptor = descriptor;
  }

private:
  int m_descriptor = -1;
};

class Sha256 final
{
public:
  Sha256() : m_context(EVP_MD_CTX_new())
  {
    m_valid = m_context != nullptr && EVP_DigestInit_ex(m_context, EVP_sha256(), nullptr) == 1;
  }
  ~Sha256()
  {
    EVP_MD_CTX_free(m_context);
  }
  Sha256(const Sha256 &) = delete;
  Sha256 &operator=(const Sha256 &) = delete;
  [[nodiscard]] bool valid() const noexcept
  {
    return m_valid && !m_finished;
  }
  bool update(std::span<const std::uint8_t> bytes)
  {
    return valid() && (bytes.empty() || EVP_DigestUpdate(m_context, bytes.data(), bytes.size()) == 1);
  }
  std::optional<FileTransferDigest> finish()
  {
    if (!valid()) {
      return std::nullopt;
    }
    FileTransferDigest digest{};
    unsigned int size = 0;
    if (EVP_DigestFinal_ex(m_context, digest.data(), &size) != 1 || size != digest.size()) {
      return std::nullopt;
    }
    m_finished = true;
    return digest;
  }

private:
  EVP_MD_CTX *m_context = nullptr;
  bool m_valid = false;
  bool m_finished = false;
};

std::uint64_t modifiedTime(const struct stat &status) noexcept
{
  const auto seconds = std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(status.st_mtimespec.tv_sec));
  const auto nanoseconds = static_cast<std::uint64_t>(status.st_mtimespec.tv_nsec);
  return seconds ^ std::rotl(nanoseconds, 17);
}

FileTransferSourceIdentity sourceIdentity(const struct stat &status)
{
  FileTransferSourceIdentity identity;
  static_assert(sizeof(status.st_dev) + sizeof(status.st_ino) <= identity.bytes.size());
  std::memcpy(identity.bytes.data(), &status.st_dev, sizeof(status.st_dev));
  std::memcpy(identity.bytes.data() + sizeof(status.st_dev), &status.st_ino, sizeof(status.st_ino));
  identity.length = sizeof(status.st_dev) + sizeof(status.st_ino);
  return identity;
}

bool matchesSource(const struct stat &status, const FileTransferSourceCandidate &source)
{
  return S_ISREG(status.st_mode) && status.st_size >= 0 && sourceIdentity(status) == source.identity &&
         static_cast<std::uint64_t>(status.st_size) == source.size && modifiedTime(status) == source.lastWriteTime;
}

std::optional<std::string> normalizedUtf8(std::string_view value)
{
  if (value.empty() || value.size() > static_cast<std::size_t>((std::numeric_limits<qsizetype>::max)())) {
    return std::nullopt;
  }
  QStringDecoder decoder(QStringDecoder::Utf8);
  auto text = decoder.decode(QByteArrayView(value.data(), static_cast<qsizetype>(value.size())));
  if (decoder.hasError()) {
    return std::nullopt;
  }
  text = text.normalized(QString::NormalizationForm_C);
  if (text.isEmpty() || text == QStringLiteral(".") || text == QStringLiteral("..") || text.contains(u'/') ||
      text.contains(u'\\') || text.contains(QChar::Null)) {
    return std::nullopt;
  }
  const auto bytes = text.toUtf8();
  return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::string transferIdHex(const TransferId &id)
{
  constexpr std::string_view digits = "0123456789abcdef";
  std::string result;
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

FileTransferWriterMutation failure(FileTransferIoError error)
{
  return {.error = error};
}

class Reader final : public IFileTransferReader
{
public:
  Reader(FileTransferSourceCandidate source, std::size_t chunkBytes, FileDescriptor file)
      : m_source(std::move(source)),
        m_chunkBytes(chunkBytes),
        m_file(std::move(file))
  {
  }

  FileTransferReadResult readNext() override
  {
    if (m_complete || m_failed) {
      return {.error = FileTransferIoError::WrongPhase};
    }
    FileTransferReadResult result;
    result.offset = m_offset;
    const auto remaining = m_source.size - m_offset;
    const auto requested = static_cast<std::size_t>((std::min)(remaining, static_cast<std::uint64_t>(m_chunkBytes)));
    result.bytes.resize(requested);

    std::size_t received = 0;
    while (received < requested) {
      const auto count = ::read(m_file.get(), result.bytes.data() + received, requested - received);
      if (count < 0 && errno == EINTR) {
        continue;
      }
      if (count <= 0) {
        m_failed = true;
        return {.error = count == 0 ? FileTransferIoError::SourceChanged : FileTransferIoError::ReadFailed};
      }
      received += static_cast<std::size_t>(count);
    }
    if (!m_hash.update(result.bytes)) {
      m_failed = true;
      return {.error = FileTransferIoError::HashFailed};
    }
    m_offset += result.bytes.size();
    if (m_offset < m_source.size) {
      return result;
    }

    struct stat status{};
    if (::fstat(m_file.get(), &status) != 0 || !matchesSource(status, m_source)) {
      m_failed = true;
      return {.error = FileTransferIoError::SourceChanged};
    }
    const auto digest = m_hash.finish();
    if (!digest.has_value()) {
      m_failed = true;
      return {.error = FileTransferIoError::HashFailed};
    }
    m_complete = true;
    result.complete = true;
    result.sha256 = *digest;
    return result;
  }

private:
  FileTransferSourceCandidate m_source;
  std::size_t m_chunkBytes = 0;
  FileDescriptor m_file;
  Sha256 m_hash;
  std::uint64_t m_offset = 0;
  bool m_complete = false;
  bool m_failed = false;
};

class Writer final : public IFileTransferWriter
{
public:
  Writer(
      FileDescriptor destination, std::filesystem::path destinationPath, FileTransferOffer offer,
      std::vector<std::string> itemNames, std::uint64_t expectedBytes
  )
      : m_destination(std::move(destination)),
        m_destinationPath(std::move(destinationPath)),
        m_offer(std::move(offer)),
        m_itemNames(std::move(itemNames)),
        m_expectedTotalBytes(expectedBytes),
        m_stagingName(".ieum-transfer-" + transferIdHex(m_offer.id))
  {
  }

  ~Writer() override
  {
    cleanupStaging();
  }

  FileTransferWriterMutation begin(const FileTransferDataBegin &message) override
  {
    if (m_state != State::AwaitingBegin) {
      return failure(FileTransferIoError::WrongPhase);
    }
    if (!routeMatches(m_offer, message.route)) {
      return fail(FileTransferIoError::RouteMismatch);
    }
    if (::mkdirat(m_destination.get(), m_stagingName.c_str(), 0700) != 0) {
      return fail(FileTransferIoError::DestinationDenied);
    }
    m_ownsStaging = true;
    m_staging.reset(
        ::openat(m_destination.get(), m_stagingName.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)
    );
    if (!m_staging.valid() || ::fchmod(m_staging.get(), 0700) != 0) {
      return fail(FileTransferIoError::DestinationDenied);
    }
    m_state = State::Receiving;
    return {};
  }

  FileTransferWriterMutation writeChunk(const FileTransferDataChunk &message) override
  {
    if (m_state != State::Receiving) {
      return failure(FileTransferIoError::WrongPhase);
    }
    if (!routeMatches(m_offer, message.route)) {
      return fail(FileTransferIoError::RouteMismatch);
    }
    if (message.itemIndex != m_currentItem) {
      return fail(FileTransferIoError::WrongItem);
    }
    if (message.offset != m_currentOffset) {
      return fail(FileTransferIoError::WrongOffset);
    }
    if (message.bytes.empty() || message.bytes.size() > kMaximumChunkBytes ||
        message.bytes.size() > std::numeric_limits<std::uint64_t>::max() - m_currentOffset ||
        m_currentOffset + message.bytes.size() > m_offer.items[m_currentItem].size) {
      return fail(FileTransferIoError::SizeExceeded);
    }
    if (!ensureCurrentFile()) {
      return fail(FileTransferIoError::DestinationDenied);
    }
    std::size_t written = 0;
    while (written < message.bytes.size()) {
      const auto count = ::write(m_currentFile.get(), message.bytes.data() + written, message.bytes.size() - written);
      if (count < 0 && errno == EINTR) {
        continue;
      }
      if (count <= 0) {
        return fail(FileTransferIoError::WriteFailed);
      }
      written += static_cast<std::size_t>(count);
    }
    if (!m_currentHash->update(message.bytes)) {
      return fail(FileTransferIoError::HashFailed);
    }
    m_currentOffset += message.bytes.size();
    m_receivedTotalBytes += message.bytes.size();
    return {};
  }

  FileTransferWriterMutation endItem(const FileTransferDataItemEnd &message) override
  {
    if (m_state != State::Receiving) {
      return failure(FileTransferIoError::WrongPhase);
    }
    if (!routeMatches(m_offer, message.route)) {
      return fail(FileTransferIoError::RouteMismatch);
    }
    if (message.itemIndex != m_currentItem) {
      return fail(FileTransferIoError::WrongItem);
    }
    const auto expectedSize = m_offer.items[m_currentItem].size;
    if (message.size != expectedSize || m_currentOffset != expectedSize) {
      return fail(FileTransferIoError::SizeExceeded);
    }
    if (!ensureCurrentFile()) {
      return fail(FileTransferIoError::DestinationDenied);
    }
    const auto digest = m_currentHash->finish();
    if (!digest.has_value()) {
      return fail(FileTransferIoError::HashFailed);
    }
    if (*digest != message.sha256) {
      return fail(FileTransferIoError::IntegrityMismatch);
    }
    if (::fsync(m_currentFile.get()) != 0) {
      return fail(FileTransferIoError::WriteFailed);
    }
    m_currentHash.reset();
    m_currentFile.reset();
    ++m_currentItem;
    m_currentOffset = 0;
    m_state = m_currentItem == m_offer.items.size() ? State::ReadyToFinish : State::Receiving;
    return {};
  }

  FileTransferWriterMutation finish(const FileTransferDataFinish &message) override
  {
    if (m_state != State::ReadyToFinish) {
      return failure(FileTransferIoError::WrongPhase);
    }
    if (!routeMatches(m_offer, message.route)) {
      return fail(FileTransferIoError::RouteMismatch);
    }
    if (message.itemCount != m_offer.items.size() || message.totalBytes != m_expectedTotalBytes ||
        m_receivedTotalBytes != m_expectedTotalBytes) {
      return fail(FileTransferIoError::SizeExceeded);
    }

    for (std::size_t item = 0; item < m_offer.items.size(); ++item) {
      const auto published = publishItem(item);
      if (!published.has_value()) {
        for (const auto &name : m_publishedNames) {
          (void)::unlinkat(m_destination.get(), name.c_str(), 0);
        }
        m_publishedNames.clear();
        m_publishedPaths.clear();
        return fail(FileTransferIoError::PublishFailed);
      }
      m_publishedNames.push_back(published->first);
      m_publishedPaths.push_back(std::move(published->second));
    }

    m_staging.reset();
    if (::unlinkat(m_destination.get(), m_stagingName.c_str(), AT_REMOVEDIR) == 0) {
      m_ownsStaging = false;
    }
    (void)::fsync(m_destination.get());
    m_state = State::Completed;
    return {.publishedPaths = m_publishedPaths};
  }

private:
  enum class State
  {
    AwaitingBegin,
    Receiving,
    ReadyToFinish,
    Completed,
    Failed,
  };

  [[nodiscard]] std::string stagedName(std::size_t item) const
  {
    return std::to_string(item) + ".part";
  }

  bool ensureCurrentFile()
  {
    if (m_currentFile.valid()) {
      return true;
    }
    const auto name = stagedName(m_currentItem);
    m_currentFile.reset(
        ::openat(m_staging.get(), name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600)
    );
    if (!m_currentFile.valid() || ::fchmod(m_currentFile.get(), 0600) != 0) {
      return false;
    }
    m_currentHash = std::make_unique<Sha256>();
    return m_currentHash->valid();
  }

  std::optional<std::pair<std::string, std::filesystem::path>> publishItem(std::size_t item)
  {
    const std::filesystem::path filename(m_itemNames[item]);
    const auto stem = filename.stem().string();
    const auto extension = filename.extension().string();
    const auto staged = stagedName(item);
    for (std::uint32_t attempt = 0; attempt < kMaximumPublishAttempts; ++attempt) {
      auto candidate = filename.string();
      if (attempt > 0) {
        candidate = stem + " (" + std::to_string(attempt) + ")" + extension;
      }
      if (::linkat(m_staging.get(), staged.c_str(), m_destination.get(), candidate.c_str(), 0) == 0) {
        if (::unlinkat(m_staging.get(), staged.c_str(), 0) != 0) {
          (void)::unlinkat(m_destination.get(), candidate.c_str(), 0);
          return std::nullopt;
        }
        return std::pair{candidate, m_destinationPath / candidate};
      }
      if (errno != EEXIST) {
        return std::nullopt;
      }
    }
    return std::nullopt;
  }

  FileTransferWriterMutation fail(FileTransferIoError error)
  {
    m_state = State::Failed;
    cleanupStaging();
    return failure(error);
  }

  void cleanupStaging() noexcept
  {
    m_currentHash.reset();
    m_currentFile.reset();
    if (!m_ownsStaging) {
      return;
    }
    if (m_staging.valid()) {
      for (std::size_t item = 0; item < m_offer.items.size(); ++item) {
        (void)::unlinkat(m_staging.get(), stagedName(item).c_str(), 0);
      }
    }
    m_staging.reset();
    if (::unlinkat(m_destination.get(), m_stagingName.c_str(), AT_REMOVEDIR) == 0) {
      m_ownsStaging = false;
    }
  }

  FileDescriptor m_destination;
  std::filesystem::path m_destinationPath;
  FileTransferOffer m_offer;
  std::vector<std::string> m_itemNames;
  std::uint64_t m_expectedTotalBytes = 0;
  std::string m_stagingName;
  FileDescriptor m_staging;
  FileDescriptor m_currentFile;
  std::unique_ptr<Sha256> m_currentHash;
  std::uint64_t m_receivedTotalBytes = 0;
  std::uint64_t m_currentOffset = 0;
  std::size_t m_currentItem = 0;
  std::vector<std::string> m_publishedNames;
  std::vector<std::filesystem::path> m_publishedPaths;
  State m_state = State::AwaitingBegin;
  bool m_ownsStaging = false;
};

} // namespace

FileTransferSourceInspection
OSXFileTransferPlatform::inspectSources(const std::vector<std::filesystem::path> &paths, std::size_t maxItems)
{
  if (paths.empty()) {
    return {.error = FileTransferIoError::EmptyPaths};
  }
  if (paths.size() > maxItems) {
    return {.error = FileTransferIoError::TooManyPaths};
  }

  std::vector<FileTransferSourceCandidate> sources;
  sources.reserve(paths.size());
  for (std::size_t position = 0; position < paths.size(); ++position) {
    const auto &path = paths[position];
    if (path.empty()) {
      return {.error = FileTransferIoError::EmptyPath, .itemPosition = position};
    }
    if (!path.is_absolute()) {
      return {.error = FileTransferIoError::RelativePath, .itemPosition = position};
    }
    const auto native = path.native();
    struct stat linkStatus{};
    if (::lstat(native.c_str(), &linkStatus) != 0) {
      return {.error = FileTransferIoError::OpenFailed, .itemPosition = position};
    }
    if (!S_ISREG(linkStatus.st_mode)) {
      return {.error = FileTransferIoError::LinkOrSpecialFile, .itemPosition = position};
    }
    FileDescriptor file(::open(native.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (!file.valid()) {
      return {.error = FileTransferIoError::OpenFailed, .itemPosition = position};
    }
    struct stat status{};
    if (::fstat(file.get(), &status) != 0) {
      return {.error = FileTransferIoError::MetadataReadFailed, .itemPosition = position};
    }
    if (!S_ISREG(status.st_mode) || status.st_size < 0 || status.st_dev != linkStatus.st_dev ||
        status.st_ino != linkStatus.st_ino) {
      return {.error = FileTransferIoError::LinkOrSpecialFile, .itemPosition = position};
    }
    const auto name = normalizedUtf8(path.filename().native());
    if (!name.has_value()) {
      return {.error = FileTransferIoError::FilenameConversionFailed, .itemPosition = position};
    }
    sources.push_back(
        FileTransferSourceCandidate{
            .path = path,
            .identity = sourceIdentity(status),
            .size = static_cast<std::uint64_t>(status.st_size),
            .lastWriteTime = modifiedTime(status),
            .name = *name,
        }
    );
  }
  return {.sources = std::move(sources)};
}

FileTransferReaderOpenResult
OSXFileTransferPlatform::openReader(FileTransferSourceCandidate source, std::size_t chunkBytes)
{
  if (source.path.empty() || !source.path.is_absolute() || source.identity.length == 0 ||
      source.identity.length > source.identity.bytes.size() || chunkBytes == 0 || chunkBytes > kMaximumChunkBytes) {
    return {.error = FileTransferIoError::SourceChanged};
  }
  FileDescriptor file(::open(source.path.native().c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (!file.valid()) {
    return {.error = FileTransferIoError::OpenFailed};
  }
  struct stat status{};
  if (::fstat(file.get(), &status) != 0 || !matchesSource(status, source)) {
    return {.error = FileTransferIoError::SourceChanged};
  }
  auto reader = std::make_unique<Reader>(std::move(source), chunkBytes, std::move(file));
  return {.reader = std::move(reader)};
}

FileTransferWriterCreateResult
OSXFileTransferPlatform::createWriter(std::filesystem::path destination, FileTransferOffer offer)
{
  const auto validation = validateOffer(offer);
  if (!validation.ok()) {
    return {.error = FileTransferIoError::InvalidOffer};
  }
  if (destination.empty() || !destination.is_absolute()) {
    return {.error = FileTransferIoError::InvalidDestination};
  }
  std::error_code directoryError;
  std::filesystem::create_directories(destination, directoryError);
  if (directoryError) {
    return {.error = FileTransferIoError::DestinationDenied};
  }
  FileDescriptor destinationFd(::open(destination.native().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (!destinationFd.valid()) {
    return {.error = FileTransferIoError::InvalidDestination};
  }
  struct stat destinationStatus{};
  if (::fstat(destinationFd.get(), &destinationStatus) != 0 || !S_ISDIR(destinationStatus.st_mode)) {
    return {.error = FileTransferIoError::InvalidDestination};
  }

  std::vector<std::string> itemNames;
  itemNames.reserve(offer.items.size());
  for (const auto &item : offer.items) {
    const auto name = normalizedUtf8(item.name);
    if (!name.has_value()) {
      return {.error = FileTransferIoError::InvalidOffer};
    }
    itemNames.push_back(*name);
  }
  auto writer = std::make_unique<Writer>(
      std::move(destinationFd), destination, std::move(offer), std::move(itemNames), validation.totalBytes
  );
  return {.writer = std::move(writer)};
}

std::optional<TransferId> OSXFileTransferPlatform::randomId()
{
  TransferId id{};
  if (SecRandomCopyBytes(kSecRandomDefault, id.size(), id.data()) != errSecSuccess ||
      std::ranges::all_of(id, [](std::uint8_t byte) { return byte == 0; })) {
    return std::nullopt;
  }
  return id;
}

} // namespace deskflow::filetransfer
