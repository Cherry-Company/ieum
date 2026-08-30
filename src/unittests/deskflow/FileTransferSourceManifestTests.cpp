/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferSourceManifest.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
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

TransferId transferId()
{
  TransferId id{};
  id[0] = 0x42;
  return id;
}

FileTransferSourceCandidate candidate(std::string name, std::uint64_t size, std::uint8_t identityByte)
{
  FileTransferSourceCandidate value;
  value.path = std::filesystem::current_path() / "source" / ("item-" + std::to_string(identityByte) + ".bin");
  value.identity.length = 1;
  value.identity.bytes[0] = identityByte;
  value.size = size;
  value.lastWriteTime = 1234 + identityByte;
  value.name = std::move(name);
  return value;
}

void buildsReceiverSafeOfferAndKeepsPathsLocal()
{
  const auto first = candidate("한글 문서.txt", 0, 1);
  const auto second = candidate("photo 01.png", 17, 2);

  const auto result = buildFileTransferSourceManifest(transferId(), "source", "target", {first, second});

  require(result.ok(), "valid source candidates should build");
  require(result.error == SourceManifestError::None, "success error should be None");
  require(result.manifest.offer.items.size() == 2, "offer should contain both files");
  require(result.manifest.sources.size() == 2, "local manifest should contain both sources");
  require(result.manifest.offer.items[0].index == 0, "first index should be zero");
  require(result.manifest.offer.items[1].index == 1, "second index should be one");
  require(result.manifest.offer.items[0].name == first.name, "offer should contain only the first basename");
  require(result.manifest.offer.items[1].name == second.name, "offer should contain only the second basename");
  require(result.manifest.offer.items[0].size == 0, "zero-byte files should remain valid");
  require(result.manifest.offer.items[1].size == 17, "offer should copy the exact size");
  require(result.manifest.sources[0] == first, "first local snapshot should be preserved");
  require(result.manifest.sources[1] == second, "second local snapshot should be preserved");
  require(result.validation.ok(), "success should retain the offer validation result");
  require(result.validation.totalBytes == 17, "success should retain the checked total");
}

void rejectsInvalidLocalSnapshotsAtomically()
{
  auto value = candidate("file.txt", 1, 1);
  value.path.clear();
  auto result = buildFileTransferSourceManifest(transferId(), "source", "target", {value});
  require(result.error == SourceManifestError::EmptySourcePath, "empty source path should fail");
  require(result.candidatePosition == 0, "empty path should identify its position");
  require(result.manifest.sources.empty(), "failure should not expose partial sources");

  value = candidate("file.txt", 1, 1);
  value.path = "relative/file.txt";
  result = buildFileTransferSourceManifest(transferId(), "source", "target", {value});
  require(result.error == SourceManifestError::RelativeSourcePath, "relative source path should fail");
  require(result.candidatePosition == 0, "relative path should identify its position");

  value = candidate("file.txt", 1, 1);
  value.identity = {};
  result = buildFileTransferSourceManifest(transferId(), "source", "target", {value});
  require(result.error == SourceManifestError::MissingFileIdentity, "missing identity should fail");
  require(result.candidatePosition == 0, "missing identity should identify its position");

  value = candidate("file.txt", 1, 1);
  value.identity.length = value.identity.bytes.size() + 1;
  result = buildFileTransferSourceManifest(transferId(), "source", "target", {value});
  require(result.error == SourceManifestError::InvalidFileIdentity, "oversized identity should fail");
  require(result.candidatePosition == 0, "invalid identity should identify its position");
}

void forwardsOfferPolicyFailures()
{
  auto id = TransferId{};
  auto result = buildFileTransferSourceManifest(id, "source", "target", {candidate("file.txt", 1, 1)});
  require(result.error == SourceManifestError::InvalidOffer, "invalid transfer ID should fail at offer policy");
  require(result.validation.error == OfferError::EmptyTransferId, "transfer ID error should be preserved");
  require(!result.candidatePosition.has_value(), "offer-level failure should not invent a candidate position");

  auto unsafe = candidate("../file.txt", 1, 1);
  result = buildFileTransferSourceManifest(transferId(), "source", "target", {unsafe});
  require(result.error == SourceManifestError::InvalidOffer, "unsafe name should fail at offer policy");
  require(result.validation.error == OfferError::UnsafeItemName, "unsafe-name error should be preserved");
  require(result.candidatePosition == 0, "item policy failure should map to the candidate position");
  require(result.manifest.offer.items.empty(), "invalid offer should not expose a partial manifest");

  FileTransferLimits limits;
  limits.maxFileBytes = 10;
  result = buildFileTransferSourceManifest(transferId(), "source", "target", {candidate("large.bin", 11, 1)}, limits);
  require(result.validation.error == OfferError::FileTooLarge, "per-file limit should be preserved");
  require(result.candidatePosition == 0, "limit failure should identify the candidate");

  result = buildFileTransferSourceManifest(transferId(), "source", "target", {});
  require(result.validation.error == OfferError::EmptyItems, "empty candidate list should use the existing offer rule");
}

} // namespace

int main()
{
  buildsReceiverSafeOfferAndKeepsPathsLocal();
  rejectsInvalidLocalSnapshotsAtomically();
  forwardsOfferPolicyFailures();
  std::cout << "PASS: FileTransferSourceManifestTests\n";
  return 0;
}
