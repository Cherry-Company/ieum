# Windows edge drop host implementation plan

**Goal:** Turn the already-tested `IDropTarget` orchestration into small,
non-activating Windows edge targets that exist only on configured sides and
resolve the destination through the server's canonical neighbour graph.

## Constraints

- Do not revive the historical full-height 20 px drag window or synthetic
  escape/mouse-release behavior.
- Register OLE drop targets only while the screen is enabled.
- Use the existing `OleInitialize` lifetime owned by `MSWindowsScreen`.
- Keep each strip at 3 px and non-activating; configured sides only.
- Avoid overlapping top/bottom strips with left/right strips at corners so one
  pointer coordinate maps to one direction.
- Preserve negative virtual-desktop coordinates and refresh geometry after a
  display-layout change.
- Resolve a destination through `Server::getNeighbor`; never accept an
  arbitrary connected screen name from the drag source.
- This change exposes metadata handoff only. Source ownership and data streaming
  are implemented by the following transfer-session change.

## Implementation

1. Add a pure edge-window layout function and tests for all directions,
   negative coordinates, partial masks, invalid bounds, and corner exclusion.
2. Add an RAII `MSWindowsEdgeDropHost` that registers a tiny topmost,
   layered, tool, no-activate window and one `MSWindowsEdgeDropTarget` per
   active direction.
3. Add `MSWindowsScreen` install/remove hooks, update them from
   `reconfigure`, display changes, enable, and disable.
4. Add a server API that maps `(direction, point)` to the same connected,
   canonical neighbour used for mouse transitions.
5. Wire callbacks only after the transfer-session owner exists, so accepted
   source handles cannot be lost between the offer and the byte stream.

## Verification

- Focused layout and host lifecycle tests on MSVC with warnings as errors.
- Existing `MSWindowsEdgeDropTargetTests` remain green.
- Full repository Windows x64/ARM64 CI before merge.
