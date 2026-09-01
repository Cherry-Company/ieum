/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/EdgeHandoffDecision.h"
#include "deskflow/FileTransferEdgeCodec.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace deskflow::server::filetransfer {

using EdgeResolver =
    std::function<std::optional<deskflow::filetransfer::EdgeTarget>(Direction, std::int32_t, std::int32_t)>;
using NeighborPredicate = std::function<bool(Direction)>;

struct EdgeRoutingResult
{
  bool authenticated = false;
  deskflow::filetransfer::FileTransferEdgeTargetResponse response;
};

[[nodiscard]] EdgeRoutingResult resolveAuthenticatedEdgeRequest(
    const deskflow::filetransfer::FileTransferEdgeTargetRequest &request, std::string_view authenticatedSource,
    const EdgeResolver &resolve
);

[[nodiscard]] std::uint32_t configuredEdgeSides(const NeighborPredicate &hasNeighbor);
[[nodiscard]] bool supportsEdgeDiscovery(std::int16_t protocolMinor) noexcept;

} // namespace deskflow::server::filetransfer
