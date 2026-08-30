# File-Transfer Control Routing Plan

**Goal:** Negotiate protocol 1.12 and safely relay bounded file-transfer control
messages between the two named screens without reviving the deprecated path-based
`DDRG`/`DFTR` implementation.

## Scope

- Add a `DFTC` outer frame with a four-byte network-order payload length.
- Refuse frames over the codec's 64 KiB cap before allocating or reading the
  advertised payload.
- Resolve message direction by role: offers originate at the source, decisions
  at the target, and cancellation/results at either participant.
- Reject third-party injection and source/target role spoofing.
- Negotiate protocol minor 1.12, add the matching server proxy, and relay valid
  control messages through the configured server connection.
- Deliver received control messages as owned event data on the local screen event
  target so the later edge/receive UI can consume them.

This slice carries metadata only. It does not open a data connection, read file
bytes, write a destination file, or enforce a paid entitlement.

## TDD sequence

1. Add focused framing and direction tests and observe the missing API failure.
2. Implement bounded frame read/write and role-aware destination resolution.
3. Add protocol 1.12 constants and client/server proxy dispatch.
4. Register sources/tests and verify GCC, MSVC, formatting, and repository CI.
