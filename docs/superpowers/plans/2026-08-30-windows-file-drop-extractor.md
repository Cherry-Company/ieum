# Windows File-Drop Extractor Implementation Plan

**Goal:** Add the first Windows-native Pro Local source adapter: safely copy
ordered Unicode file paths out of a `CF_HDROP` handle without retaining COM
objects or touching the filesystem.

**Scope:** This slice only decodes the native drag payload. It does not create
edge windows, cancel a native drag, stat files, build an offer, send protocol
messages, or write received files. Those remain separate reviewable slices.

## Production contract

Add `MSWindowsFileDropExtractor.h/.cpp` under `src/lib/platform`.

The extractor:

- accepts an `HDROP` and configurable item/path-character limits;
- preserves native item order and exact Unicode paths;
- supports more than one file and paths longer than `MAX_PATH`;
- rejects a null handle, an empty drop, too many items, an empty/unreadable
  path, and a path over the configured character limit;
- returns no partial path list when any item fails;
- copies path strings immediately and never retains `HDROP`, `IDataObject`, or
  `STGMEDIUM` ownership.

## Task 1: Write and observe the failing Windows test

**Files:**

- Create: `src/unittests/platform/MSWindowsFileDropExtractorTests.cpp`
- Modify: `src/unittests/platform/CMakeLists.txt`

The test builds real movable `DROPFILES` blocks in memory and covers:

1. null and empty handles;
2. one Unicode path;
3. multiple paths in stable order;
4. a path beyond legacy `MAX_PATH`;
5. exact item and path-limit boundaries;
6. atomic rejection when any path is over its limit.

Compile the test before creating the production header and confirm that the
compiler fails specifically because `MSWindowsFileDropExtractor.h` is absent.

## Task 2: Implement the minimum extractor

**Files:**

- Create: `src/lib/platform/MSWindowsFileDropExtractor.h`
- Create: `src/lib/platform/MSWindowsFileDropExtractor.cpp`
- Modify: `src/lib/platform/CMakeLists.txt`

Use `DragQueryFileW` twice per item: once for the required character count and
once to copy the path. Bound the count before allocation and require the second
query to return the same length. On failure, return an error and an empty path
vector. Link `shell32` explicitly on Windows.

Compile and run the focused test with the local Visual Studio Build Tools, then
let the repository Windows jobs verify the normal CMake/Qt-linked target.

## Task 3: Deliver the isolated slice

Run `git diff --check`, inspect only this branch's diff, commit with the public
GitHub noreply identity, push, open a PR against `ieum/main`, and wait for the
repository CI gate before merging. A follow-up slice will adapt `IDataObject`
ownership and connect extracted candidates to the neutral offer builder.
