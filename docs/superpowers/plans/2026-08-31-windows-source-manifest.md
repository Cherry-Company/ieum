# Windows File-Transfer Source Manifest Plan

**Goal:** Convert the paths acquired from a Windows `CF_HDROP` into a stable,
local-only source snapshot and a receiver-safe transfer offer.

**Scope:** This slice opens no network connection and shows no UI. It only
bridges the already extracted paths to the existing offer contract.

## Contract

- The Windows inspector accepts only absolute, existing, non-directory disk
  files and rejects reparse points.
- Each accepted source records its local path, byte size, last-write time, and
  stable Windows file identity. These fields remain local and are compared
  again when streaming starts.
- The platform-neutral builder copies only the item index, UTF-8 basename, and
  byte size into `FileTransferOffer`.
- The existing `FileTransferOfferValidator` remains the final policy gate.
- Any failure is atomic: no partial candidate list or partial manifest is
  returned.

## TDD sequence

1. Add focused tests for source/offer separation, ordering, identity, limits,
   missing paths, relative paths, directories, and validator forwarding.
2. Observe both focused test builds fail because the source contracts do not
   exist.
3. Add the minimal neutral manifest builder and Windows file inspector.
4. Run the neutral test and Windows test independently with warnings treated
   as errors.
5. Register the sources and tests in CMake, format the touched C++ files, and
   rerun the two focused tests.
