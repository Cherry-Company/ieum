# Pro Local File Transfer Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the validated file-offer contract and deterministic screen-edge handoff state machine that the Windows adapter and protocol 1.12 implementation will consume.

**Architecture:** Keep this first merge platform neutral and inert: it neither opens files nor sends network messages. `FileTransferOfferValidator` owns all untrusted manifest checks, while `EdgeHandoffDecision` owns only edge dwell and release decisions; operating-system drag extraction, protocol serialization, and file streaming remain later plans.

**Tech Stack:** C++20, existing `base::Unicode` UTF-8 validation, Qt Test, CMake, MSVC/Clang/GCC

**Spec:** `docs/superpowers/specs/2026-08-30-pro-local-edge-file-transfer-design.md`

## Global Constraints

- Keep `GPL-2.0-only WITH LicenseRef-OpenSSL-Exception` and preserve existing notices.
- Source absolute paths never enter `FileTransferItem` or `FileTransferOffer`.
- First-beta defaults are 100 items, 1,024 UTF-8 bytes per name, 20 GiB per file, and 50 GiB aggregate.
- Only regular-file display names are represented: no path separator, traversal component, device name, trailing dot, or trailing space.
- Zero-byte files are valid.
- The edge dwell is exactly 250 ms by default.
- No protocol version bump, GUI IPC change, filesystem access, or network I/O belongs in this plan.
- Every production behavior is preceded by a test that is observed failing for the missing behavior.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/lib/deskflow/FileTransferTypes.h` | Plain value types, limits, and validation result enums for a transfer offer |
| `src/lib/deskflow/FileTransferOfferValidator.h` | Public validation interface |
| `src/lib/deskflow/FileTransferOfferValidator.cpp` | Checked totals, UTF-8, safe-name, identity, and limit validation |
| `src/lib/deskflow/EdgeHandoffDecision.h` | Edge target, state, and deterministic time-based interface |
| `src/lib/deskflow/EdgeHandoffDecision.cpp` | Candidate, armed, release, target-change, and reset transitions |
| `src/unittests/deskflow/FileTransferOfferTests.cpp` | Offer acceptance and every rejection boundary |
| `src/unittests/deskflow/EdgeHandoffDecisionTests.cpp` | Edge dwell and release behavior |
| `src/lib/deskflow/CMakeLists.txt` | Add the four production files to `app` |
| `src/unittests/deskflow/CMakeLists.txt` | Register the two focused test executables |

### Task 1: Validated file-transfer offer contract

**Files:**

- Create: `src/lib/deskflow/FileTransferTypes.h`
- Create: `src/lib/deskflow/FileTransferOfferValidator.h`
- Create: `src/lib/deskflow/FileTransferOfferValidator.cpp`
- Create: `src/unittests/deskflow/FileTransferOfferTests.cpp`
- Modify: `src/lib/deskflow/CMakeLists.txt`
- Modify: `src/unittests/deskflow/CMakeLists.txt`

**Interfaces:**

- Consumes: `Unicode::isUTF8(const std::string&)` from `base/Unicode.h`.
- Produces:

```cpp
namespace deskflow::filetransfer {

using TransferId = std::array<std::uint8_t, 16>;

struct FileTransferItem
{
  std::uint32_t index = 0;
  std::string name;
  std::uint64_t size = 0;
};

struct FileTransferOffer
{
  TransferId id{};
  std::string sourceScreen;
  std::string targetScreen;
  std::vector<FileTransferItem> items;
};

struct FileTransferLimits
{
  std::size_t maxItems = 100;
  std::size_t maxNameBytes = 1024;
  std::size_t maxScreenNameBytes = 255;
  std::uint64_t maxFileBytes = 20ULL * 1024 * 1024 * 1024;
  std::uint64_t maxTotalBytes = 50ULL * 1024 * 1024 * 1024;
};

enum class OfferError
{
  None,
  EmptyTransferId,
  EmptySourceScreen,
  EmptyTargetScreen,
  SameSourceAndTarget,
  ScreenNameTooLong,
  InvalidScreenNameUtf8,
  UnsafeScreenName,
  EmptyItems,
  TooManyItems,
  NonSequentialItemIndex,
  EmptyItemName,
  ItemNameTooLong,
  InvalidItemNameUtf8,
  UnsafeItemName,
  FileTooLarge,
  TotalSizeOverflow,
  TotalSizeTooLarge,
};

struct OfferValidation
{
  OfferError error = OfferError::None;
  std::optional<std::size_t> itemPosition;
  std::uint64_t totalBytes = 0;

  [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] OfferValidation
validateOffer(const FileTransferOffer &offer, const FileTransferLimits &limits = {});

} // namespace deskflow::filetransfer
```

- `itemPosition` is populated only for an item-specific error and is the vector position, not untrusted `item.index`.
- A valid item index equals its vector position; this gives the later wire decoder one unambiguous order and rejects duplicates and gaps together.

- [x] **Step 1: Write the failing offer tests**

Create `FileTransferOfferTests.cpp` with a real `validOffer()` fixture and these test slots:

```cpp
class FileTransferOfferTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void acceptsUnicodeAndZeroByteFiles();
  void rejectsMissingIdentityAndInvalidScreens_data();
  void rejectsMissingIdentityAndInvalidScreens();
  void rejectsEmptyAndOversizedItemLists();
  void rejectsNonSequentialIndices_data();
  void rejectsNonSequentialIndices();
  void rejectsUnsafeNames_data();
  void rejectsUnsafeNames();
  void rejectsInvalidUtf8AndNameLength();
  void rejectsPerFileAndAggregateLimits();
  void detectsTotalSizeOverflow();
};
```

The hand-derived fixtures must include:

```cpp
FileTransferOffer validOffer()
{
  FileTransferOffer offer;
  offer.id[0] = 0x42;
  offer.sourceScreen = "source";
  offer.targetScreen = "target";
  offer.items = {
      {0, "한글.txt", 0},
      {1, "photo 01.png", 17},
  };
  return offer;
}
```

Unsafe-name data rows must cover `""`, `"."`, `".."`, `"../a"`,
`"a/b"`, `"a\\b"`, an embedded NUL, ASCII control input,
`"bad<name.txt"`, `"trailing."`, `"trailing "`, `"CON"`,
`"con.txt"`, `"NUL.bin"`, `"COM1"`, and `"LPT9.log"`. Include valid
counterexamples `"console.txt"`, `"COM10.txt"`, and UTF-8 Korean text so
reserved-name logic cannot reject by prefix.

Screen identity rows must also cover an embedded NUL and ASCII control input;
these return `UnsafeScreenName`. Screen names are identifiers, not paths, so
ordinary Unicode, spaces, and punctuation other than NUL/control bytes remain
valid.

For each failure, compare the exact `OfferError`, and for item failures compare
the literal expected `itemPosition`. For valid input, compare the literal total
`17`.

- [x] **Step 2: Run the offer test and verify RED**

Use the WSL Qt Test installation to compile the focused test independently of
the repository's newer full-build Qt floor:

```bash
repo=/mnt/c/Users/LYR/Documents/GitHub/ieum-pro-file-transfer
tmp=$(mktemp -d)
/usr/lib/qt6/libexec/moc \
  "$repo/src/unittests/deskflow/FileTransferOfferTests.cpp" \
  -o "$tmp/FileTransferOfferTests.moc"
g++ -std=c++20 -fPIC \
  $(pkg-config --cflags Qt6Test) \
  -I"$repo/src/lib" -I"$tmp" \
  "$repo/src/unittests/deskflow/FileTransferOfferTests.cpp" \
  "$repo/src/lib/deskflow/FileTransferOfferValidator.cpp" \
  "$repo/src/lib/base/Unicode.cpp" \
  $(pkg-config --libs Qt6Test) \
  -o "$tmp/FileTransferOfferTests"
```

Expected: compile failure because the offer types and validator do not exist.
This is the named break: removing or omitting validation leaves the public
contract unavailable.

- [x] **Step 3: Add the minimal offer types and validator**

Implement only the published interface. Validation order is deterministic:

1. nonzero transfer ID;
2. nonempty, bounded, valid-UTF-8 source and target identities with no NUL or
   ASCII control byte;
3. different source and target;
4. nonempty and bounded item list;
5. for each item: sequential index, nonempty/bounded/valid-UTF-8 safe name,
   per-file limit, and checked total;
6. aggregate limit.

The safe-name helper remains private to the `.cpp`. Reject ASCII bytes below
`0x20`, `0x7f`, `<>:\"/\\|?*`, trailing dot or space, and a Windows device
basename (case-insensitive `CON`, `PRN`, `AUX`, `NUL`, `COM1`-`COM9`, or
`LPT1`-`LPT9`) before the first dot. Do not normalize or rewrite a name; either
accept the exact UTF-8 name or reject it.

Checked addition must be explicit:

```cpp
if (item.size > std::numeric_limits<std::uint64_t>::max() - totalBytes) {
  return itemFailure(OfferError::TotalSizeOverflow, position, totalBytes);
}
totalBytes += item.size;
```

- [x] **Step 4: Run the focused offer test and verify GREEN**

Repeat the compile command from Step 2, then run:

```bash
"$tmp/FileTransferOfferTests" -o -,txt
```

Expected: all offer tests pass, with zero warnings or crashes.

- [x] **Step 5: Register production and test files in CMake**

Add the types and validator `.h/.cpp` files to the `app` source list. Register
`FileTransferOfferTests` with:

```cmake
create_test(
  NAME FileTransferOfferTests
  DEPENDS app
  LIBS arch base ${extra_libs}
  SOURCE FileTransferOfferTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/deskflow"
)
```

- [x] **Step 6: Format and commit Task 1**

Run the repository clang-format style on the new C++ files, rerun the focused
test, then commit:

```bash
git add \
  src/lib/deskflow/FileTransferTypes.h \
  src/lib/deskflow/FileTransferOfferValidator.h \
  src/lib/deskflow/FileTransferOfferValidator.cpp \
  src/lib/deskflow/CMakeLists.txt \
  src/unittests/deskflow/FileTransferOfferTests.cpp \
  src/unittests/deskflow/CMakeLists.txt
git commit -m "feat(file-transfer): validate transfer offers"
```

### Task 2: Deterministic edge-handoff decision

**Files:**

- Create: `src/lib/deskflow/EdgeHandoffDecision.h`
- Create: `src/lib/deskflow/EdgeHandoffDecision.cpp`
- Create: `src/unittests/deskflow/EdgeHandoffDecisionTests.cpp`
- Modify: `src/lib/deskflow/CMakeLists.txt`
- Modify: `src/unittests/deskflow/CMakeLists.txt`

**Interfaces:**

- Consumes: global `Direction` from `base/DirectionTypes.h` and a target screen name supplied later by the configured screen-layout adapter.
- Produces:

```cpp
namespace deskflow::filetransfer {

struct EdgeTarget
{
  Direction direction = Direction::NoDirection;
  std::string screen;
  bool operator==(const EdgeTarget &) const = default;
};

enum class EdgeHandoffState
{
  Idle,
  Candidate,
  Armed,
};

class EdgeHandoffDecision
{
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  explicit EdgeHandoffDecision(std::chrono::milliseconds dwell = std::chrono::milliseconds{250});

  [[nodiscard]] EdgeHandoffState observe(const std::optional<EdgeTarget> &target, TimePoint now);
  [[nodiscard]] std::optional<EdgeTarget> release();
  void cancel() noexcept;

  [[nodiscard]] EdgeHandoffState state() const noexcept;
  [[nodiscard]] const std::optional<EdgeTarget> &target() const noexcept;
};

} // namespace deskflow::filetransfer
```

- A valid target has a nonempty screen and one of Left, Right, Top, or Bottom.
- `observe(nullopt, now)` and an invalid target both reset to Idle.
- The first observation starts Candidate. The same target at exactly the dwell threshold becomes Armed.
- Changing direction or screen restarts Candidate at the new timestamp.
- A timestamp earlier than the previous observation restarts Candidate rather than arming from a negative or wrapped duration.
- `release()` returns the target only from Armed and always resets to Idle.

- [x] **Step 1: Write the failing edge-decision tests**

Create tests for these independent behaviors:

```cpp
void startsAsIdle();
void armsOnlyAtTheDwellBoundary();
void movingAwayDisarms();
void changingTargetRestartsDwell();
void invalidTargetsNeverArm_data();
void invalidTargetsNeverArm();
void releaseBeforeArmedReturnsNothingAndResets();
void releaseWhenArmedReturnsTargetAndResets();
void backwardsClockRestartsDwell();
void customDwellIsHonored();
```

Use `TimePoint{} + 249ms`, `250ms`, and literal target values. Do not sleep in a
test and do not read the real clock.

- [x] **Step 2: Run the edge test and verify RED**

```bash
repo=/mnt/c/Users/LYR/Documents/GitHub/ieum-pro-file-transfer
tmp=$(mktemp -d)
/usr/lib/qt6/libexec/moc \
  "$repo/src/unittests/deskflow/EdgeHandoffDecisionTests.cpp" \
  -o "$tmp/EdgeHandoffDecisionTests.moc"
g++ -std=c++20 -fPIC \
  $(pkg-config --cflags Qt6Test) \
  -I"$repo/src/lib" -I"$tmp" \
  "$repo/src/unittests/deskflow/EdgeHandoffDecisionTests.cpp" \
  "$repo/src/lib/deskflow/EdgeHandoffDecision.cpp" \
  $(pkg-config --libs Qt6Test) \
  -o "$tmp/EdgeHandoffDecisionTests"
```

Expected: compile failure because `EdgeHandoffDecision` does not exist.

- [x] **Step 3: Implement the minimal state machine**

Store the state, current optional target, candidate start, previous observation
time, dwell, and whether a timestamp has been observed. Do not add callbacks,
Qt signals, filesystem state, screen geometry, or protocol methods.

Constructor behavior for a negative dwell is to clamp it to zero. A zero dwell
arms on the first valid observation. This prevents a negative duration from
creating platform-dependent results.

- [x] **Step 4: Run the focused edge test and verify GREEN**

Repeat Step 2 and run:

```bash
"$tmp/EdgeHandoffDecisionTests" -o -,txt
```

Expected: all edge-decision tests pass.

- [x] **Step 5: Register production and test files in CMake**

Add the edge `.h/.cpp` files to `app`. Register the test:

```cmake
create_test(
  NAME EdgeHandoffDecisionTests
  DEPENDS app
  LIBS arch base ${extra_libs}
  SOURCE EdgeHandoffDecisionTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/deskflow"
)
```

- [x] **Step 6: Run both focused tests, format, and commit Task 2**

Run both WSL test executables again after formatting. Then commit:

```bash
git add \
  src/lib/deskflow/EdgeHandoffDecision.h \
  src/lib/deskflow/EdgeHandoffDecision.cpp \
  src/lib/deskflow/CMakeLists.txt \
  src/unittests/deskflow/EdgeHandoffDecisionTests.cpp \
  src/unittests/deskflow/CMakeLists.txt
git commit -m "feat(file-transfer): decide screen-edge handoffs"
```

### Task 3: Publish the approved product and cost boundary

**Files:**

- Create: `docs/superpowers/specs/2026-08-30-pro-local-edge-file-transfer-design.md`
- Modify: `README.md`
- Modify: `README.en.md`
- Modify: `README.zh-CN.md`
- Modify: `docs/product/commercialization-roadmap.md`

**Interfaces:**

- Consumes: the approved A-model, selected edge gesture, and current GitHub audit count of 16 closed / 17 open.
- Produces: one canonical technical design linked by every public README and the commercialization roadmap.

- [x] **Step 1: Review the rendered documentation contract**

Confirm all four properties directly in the rendered source:

1. Pro Local direct bytes do not traverse an Ieum-operated server.
2. Pro Cloud relay is subscription and quota based.
3. Desktop source and protocol stay GPL.
4. The first beta writes to a receiver-approved `Ieum Transfers` directory;
   arbitrary native remote drop is later.

- [x] **Step 2: Check links, stale counts, placeholders, and whitespace**

Run:

```bash
git diff --check
rg -n "15/33|18 remain|T[B]D|T[O]DO|implement [l]ater|fill in [d]etails" \
  README.md README.en.md README.zh-CN.md \
  docs/product/commercialization-roadmap.md \
  docs/superpowers/specs/2026-08-30-pro-local-edge-file-transfer-design.md
```

Expected: `git diff --check` succeeds and the search returns no stale roadmap
count or plan placeholder in the changed sections.

- [x] **Step 3: Commit Task 3**

```bash
git add \
  README.md README.en.md README.zh-CN.md \
  docs/product/commercialization-roadmap.md \
  docs/superpowers/specs/2026-08-30-pro-local-edge-file-transfer-design.md \
  docs/superpowers/plans/2026-08-30-pro-local-file-transfer-foundation.md
git commit -m "docs: define Pro Local file-transfer boundary"
```

### Task 4: Branch verification and delivery

**Files:** No new files; verify the complete branch.

**Interfaces:**

- Consumes: Tasks 1–3.
- Produces: reviewable branch with two passing focused test executables and clean documentation.

- [ ] **Step 1: Re-run focused behavior tests**

Rebuild and run `FileTransferOfferTests` and `EdgeHandoffDecisionTests` with the
WSL commands above. Expected: both pass.

- [ ] **Step 2: Run source and history checks**

```bash
git diff --check origin/ieum/main...HEAD
git status --short
git log --oneline origin/ieum/main..HEAD
```

Expected: no whitespace error, no uncommitted file, and exactly the intentional
feature/documentation commits.

- [ ] **Step 3: Push, open a PR, and use repository CI as the full-build gate**

```bash
git push -u origin feat/pro-file-transfer-edge-drag
gh pr create \
  --repo Cherry-Company/ieum \
  --base ieum/main \
  --head feat/pro-file-transfer-edge-drag \
  --title "feat: lay Pro Local file-transfer foundation" \
  --body "Adds the validated Pro Local transfer-offer contract, the deterministic 250 ms edge-handoff decision, and the approved direct-versus-relay product boundary. Focused FileTransferOfferTests and EdgeHandoffDecisionTests pass under WSL Qt Test; repository CI supplies the full supported-platform build gate."
```

The PR body records that the local full CMake baseline was unavailable because
the host shell has no configured Qt 6.7+ installation; it also records the two
focused WSL Qt Test commands and results. Required repository CI is the
cross-platform compile and test authority.

- [ ] **Step 4: Merge only after required checks pass**

Use the repository's normal squash merge. Verify `origin/ieum/main` contains
the merge commit, then delete only the feature branch if the merge operation
does not do so automatically. Do not modify the user's original
`fix/clean-state-settings` worktree or its untracked specification file.
