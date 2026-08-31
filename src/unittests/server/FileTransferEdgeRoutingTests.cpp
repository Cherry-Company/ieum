/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/FileTransferEdgeRouting.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

using namespace deskflow::filetransfer;
using namespace deskflow::server::filetransfer;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

TransferId transferId(std::uint8_t first = 0x31)
{
  TransferId id{};
  id[0] = first;
  return id;
}

FileTransferEdgeTargetRequest request(std::string source = "windows-client")
{
  return {
      .requestId = transferId(),
      .sourceScreen = std::move(source),
      .direction = Direction::Left,
      .x = 0,
      .y = 200,
  };
}

void rejectsSpoofedSource()
{
  bool resolverCalled = false;
  const auto routed = resolveAuthenticatedEdgeRequest(
      request("other-client"), "windows-client",
      [&resolverCalled](Direction, std::int32_t, std::int32_t) -> std::optional<EdgeTarget> {
        resolverCalled = true;
        return EdgeTarget{.direction = Direction::Left, .screen = "macbook"};
      }
  );
  require(!routed.authenticated, "claimed source must match the authenticated peer");
  require(!resolverCalled, "spoofed requests must not reach topology lookup");
  require(routed.response.status == FileTransferEdgeStatus::Rejected, "spoofed request must be rejected");
}

void resolvesCanonicalNeighbor()
{
  const auto routed = resolveAuthenticatedEdgeRequest(
      request(), "windows-client",
      [](Direction direction, std::int32_t x, std::int32_t y) -> std::optional<EdgeTarget> {
        require(direction == Direction::Left && x == 0 && y == 200, "resolver receives exact edge coordinates");
        return EdgeTarget{.direction = direction, .screen = "macbook"};
      }
  );
  require(routed.authenticated, "canonical source should authenticate");
  require(routed.response.status == FileTransferEdgeStatus::Resolved, "configured neighbor should resolve");
  require(routed.response.targetScreen == "macbook", "response should use canonical target");
}

void reportsUnconfiguredEdge()
{
  const auto routed =
      resolveAuthenticatedEdgeRequest(request(), "windows-client", [](Direction, std::int32_t, std::int32_t) {
        return std::optional<EdgeTarget>{};
      });
  require(routed.authenticated, "canonical no-target request should authenticate");
  require(routed.response.status == FileTransferEdgeStatus::NoTarget, "unconfigured edge should report no target");
  require(routed.response.targetScreen.empty(), "no-target response must not name a target");
}

void computesCapabilitiesAndVersionGate()
{
  const auto sides = configuredEdgeSides([](Direction direction) {
    return direction == Direction::Left || direction == Direction::Bottom;
  });
  const auto expected =
      static_cast<std::uint32_t>(DirectionMask::LeftMask) | static_cast<std::uint32_t>(DirectionMask::BottomMask);
  require(sides == expected, "capabilities should include only configured sides");
  require(!supportsEdgeDiscovery(13), "protocol 1.13 must not receive DFTE");
  require(supportsEdgeDiscovery(14), "protocol 1.14 must receive DFTE");
}

} // namespace

int main()
{
  rejectsSpoofedSource();
  resolvesCanonicalNeighbor();
  reportsUnconfiguredEdge();
  computesCapabilitiesAndVersionGate();
  return 0;
}
