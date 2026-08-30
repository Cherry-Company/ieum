/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferSourceManifest.h"

#include "deskflow/FileTransferOfferValidator.h"

#include <cstdint>
#include <limits>
#include <utility>

namespace deskflow::filetransfer {
namespace {

FileTransferSourceManifestResult sourceFailure(SourceManifestError error, std::size_t position)
{
  return {
      .error = error,
      .candidatePosition = position,
      .validation = {},
      .manifest = {},
  };
}

FileTransferSourceManifestResult offerFailure(const OfferValidation &validation)
{
  return {
      .error = SourceManifestError::InvalidOffer,
      .candidatePosition = validation.itemPosition,
      .validation = validation,
      .manifest = {},
  };
}

} // namespace

FileTransferSourceManifestResult buildFileTransferSourceManifest(
    TransferId id, std::string sourceScreen, std::string targetScreen,
    std::vector<FileTransferSourceCandidate> candidates, const FileTransferLimits &limits
)
{
  for (std::size_t position = 0; position < candidates.size(); ++position) {
    const auto &candidate = candidates[position];
    if (candidate.path.empty()) {
      return sourceFailure(SourceManifestError::EmptySourcePath, position);
    }
    if (!candidate.path.is_absolute()) {
      return sourceFailure(SourceManifestError::RelativeSourcePath, position);
    }
    if (candidate.identity.length == 0) {
      return sourceFailure(SourceManifestError::MissingFileIdentity, position);
    }
    if (candidate.identity.length > candidate.identity.bytes.size()) {
      return sourceFailure(SourceManifestError::InvalidFileIdentity, position);
    }
  }

  FileTransferOffer offer;
  offer.id = id;
  offer.sourceScreen = std::move(sourceScreen);
  offer.targetScreen = std::move(targetScreen);
  offer.items.reserve(candidates.size());

  for (std::size_t position = 0; position < candidates.size(); ++position) {
    if (position > std::numeric_limits<std::uint32_t>::max()) {
      OfferValidation validation;
      validation.error = OfferError::TooManyItems;
      return offerFailure(validation);
    }

    const auto &candidate = candidates[position];
    offer.items.push_back({static_cast<std::uint32_t>(position), candidate.name, candidate.size});
  }

  const auto validation = validateOffer(offer, limits);
  if (!validation.ok()) {
    return offerFailure(validation);
  }

  FileTransferSourceManifest manifest;
  manifest.offer = std::move(offer);
  manifest.sources = std::move(candidates);
  return {
      .error = SourceManifestError::None,
      .candidatePosition = std::nullopt,
      .validation = validation,
      .manifest = std::move(manifest),
  };
}

} // namespace deskflow::filetransfer
