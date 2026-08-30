# Windows Data-Object Bridge Plan

**Goal:** Copy a `CF_HDROP` payload out of an OLE `IDataObject` and release the
returned `STGMEDIUM` on every successful acquisition path.

## Contract

- Request only `CF_HDROP`, `DVASPECT_CONTENT`, index `-1`, and
  `TYMED_HGLOBAL`.
- Distinguish null object, unsupported format, failed acquisition, and invalid
  storage from native path-decoding errors.
- Call `ReleaseStgMedium` exactly once after every successful `GetData`, even
  when the medium or decoded payload is rejected.
- Copy paths before releasing the medium and retain no COM pointer or handle.
- Forward the existing item/path limits unchanged.

## TDD sequence

1. Extend `MSWindowsFileDropExtractorTests.cpp` with a fake `IDataObject` and a
   release-tracking `IUnknown`.
2. Observe compilation fail because the data-object bridge API and errors do
   not exist.
3. Add the minimum overload in `MSWindowsFileDropExtractor.h/.cpp`.
4. Compile with MSVC C++20, `/W4 /WX /permissive- /utf-8`, then run the focused
   executable.
5. Format, repeat the focused test, rebase the new commit onto fetched
   `origin/ieum/main`, and use the repository CI gate before merging.
