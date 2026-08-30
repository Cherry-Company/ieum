# File-transfer session state implementation plan

**Goal:** Own outgoing source snapshots and incoming offers across the control
handshake, with explicit role and phase validation before any file is opened.

## State model

```text
outgoing: AwaitingDecision -> ReadyToSend -> Sending -> terminal
                           \-> Rejected

incoming: AwaitingDecision -> AwaitingData -> Receiving -> terminal
                           \-> Rejected

terminal: Completed | Failed | Cancelled | Rejected
```

## Rules

- A transfer ID is unique across both incoming and outgoing sessions.
- Outgoing sessions retain the complete, validated source manifest.
- Incoming sessions retain metadata only.
- The local screen must be the source for outgoing sessions and the target for
  incoming sessions.
- Decisions may be made once and may only arrive at the source.
- Control messages must match the original route byte-for-byte.
- Cancellation and results are accepted only for a known, non-terminal session.
- The registry has a fixed concurrent-session cap and fails closed.
- Terminal sessions can be explicitly erased; source snapshots are then
  released.

## Verification

- Focused C++20 tests cover accept/reject, duplicate IDs, route spoofing,
  unknown IDs, wrong phases, cancellation/results, capacity, and cleanup.
- GCC and MSVC compile with warnings as errors.
