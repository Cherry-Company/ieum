# File-Transfer Control Codec Plan

**Goal:** Define the bounded binary control payload that protocol 1.12 routing
will carry for offers, decisions, cancellation, and terminal results.

## Scope

- Add platform-neutral control value types and one versioned binary codec.
- Encode integers in network byte order and preserve UTF-8 names exactly.
- Cap the entire payload at 64 KiB by default.
- Reuse the existing offer and route policy checks after decoding.
- Reject unknown versions, message kinds, flags, enum values, oversized counts
  or strings, truncation, trailing bytes, and invalid manifests.

This slice does not raise the negotiated protocol minor version, register wire
dispatch handlers, open a transfer stream, or route a message between peers.
The version bump occurs only when both client and server dispatch paths exist.

## TDD sequence

1. Add focused round-trip, deterministic-byte, truncation, mutation, size, and
   policy tests.
2. Observe compile failure because the control types and codec do not exist.
3. Expose the common transfer-route validator used by both offer and control
   validation.
4. Implement the minimal bounded reader/writer and the four control payloads.
5. Register the sources and test, format with clang-format 20.1.0, and rerun
   with warnings as errors.
