/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/FileTransferEdgeRouting.h"

#include "deskflow/ProtocolTypes.h"

namespace deskflow::server::filetransfer {

EdgeRoutingResult resolveAuthenticatedEdgeRequest(
    const deskflow::filetransfer::FileTransferEdgeTargetRequest &request, std::string_view authenticatedSource,
    const EdgeResolver &resolve
)
{
  EdgeRoutingResult result{
      .authenticated = request.sourceScreen == authenticatedSource,
      .response = {
          .requestId = request.requestId,
          .sourceScreen = request.sourceScreen,
          .status = deskflow::filetransfer::FileTransferEdgeStatus::Rejected,
          .targetScreen = {},
      },
  };
  if (!result.authenticated) {
    return result;
  }

  const auto target = resolve(request.direction, request.x, request.y);
  result.response.status = target ? deskflow::filetransfer::FileTransferEdgeStatus::Resolved
                                  : deskflow::filetransfer::FileTransferEdgeStatus::NoTarget;
  if (target) {
    result.response.targetScreen = target->screen;
  }
  return result;
}

std::uint32_t configuredEdgeSides(const NeighborPredicate &hasNeighbor)
{
  using enum Direction;
  using enum DirectionMask;
  std::uint32_t sides = 0;
  if (hasNeighbor(Left)) {
    sides |= static_cast<std::uint32_t>(LeftMask);
  }
  if (hasNeighbor(Right)) {
    sides |= static_cast<std::uint32_t>(RightMask);
  }
  if (hasNeighbor(Top)) {
    sides |= static_cast<std::uint32_t>(TopMask);
  }
  if (hasNeighbor(Bottom)) {
    sides |= static_cast<std::uint32_t>(BottomMask);
  }
  return sides;
}

bool supportsEdgeDiscovery(std::int16_t protocolMinor) noexcept
{
  return protocolMinor >= kProtocolFileTransferEdgeMinorVersion;
}

} // namespace deskflow::server::filetransfer
