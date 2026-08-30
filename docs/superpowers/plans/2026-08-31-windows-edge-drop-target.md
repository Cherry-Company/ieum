# Windows Edge OLE Drop Target Plan

**Goal:** Turn a Windows OLE file drag into a bounded, dwell-gated edge
handoff without retaining the source `IDataObject` or faking input release.

## Scope

- Add an `IDropTarget` orchestration object in the Windows application layer.
- Load native file candidates on `DragEnter`, then retain only local snapshot
  values.
- Resolve the current pointer to an injected configured `EdgeTarget` and use
  the existing 250 ms `EdgeHandoffDecision`.
- Advertise copy only while armed and only when the drag source permits copy.
- Re-snapshot on `Drop`; reject if path, file identity, size, modification
  time, or name changed during the drag.
- Publish state for a later overlay host and emit a handoff callback only for
  a valid armed release.

This slice does not create edge windows, resolve server configuration, send a
protocol offer, or move bytes.

## TDD sequence

1. Add focused COM tests for lifetime, candidate/armed timing, move-away and
   leave cancellation, copy-effect negotiation, changed snapshots, unsafe
   candidates, callbacks, and no `IDataObject` retention.
2. Observe compile failure because the drop target does not exist.
3. Add the minimal target with injected clock, target resolver, state callback,
   source loader, and handoff callback.
4. Compile with MSVC C++20, `/permissive- /W4 /WX /utf-8`, then run all focused
   cases.
5. Register the source and test in Windows-only CMake lists, format, and rerun.
