# Secure Socket Write Reliability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close issue #6 by making TLS write retry bytes connection-local, preserving the existing allocation on growth failure, and resetting the Windows writable-poll hint after `SSL_ERROR_WANT_WRITE`.

**Architecture:** A focused `SecureSocketWriteBuffer` value owns one connection's retry payload and exposes a realloc-compatible injection point so allocation failure is reproducible in a unit test. `SecureSocket` delegates retry-buffer ownership to that value. `IArchNetwork` gains a default no-op writable-poll reset, while the WinSock implementation resets its cached poll flag.

**Tech Stack:** C++20, Qt 6 Test, OpenSSL, CMake/Ninja, WinSock, Python 3 code-graph tests.

**Spec:** `docs/superpowers/specs/2026-08-23-production-commercialization-roadmap-design.md`

## Global Constraints

- Keep `GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`; add SPDX headers to every new source file.
- Do not modify, stage, or commit `deskflow_kr_ime_spec_v0.1.md`.
- Preserve the output bytes and recorded size when buffer growth fails.
- One `SecureSocket` instance must never reuse another instance's retry flag, size, capacity, or bytes.
- `SSL_ERROR_WANT_WRITE` must request a fresh WinSock writable notification before the socket is marked writable again.
- Unix implementations retain a safe default no-op poll-reset method.
- Begin each behavior with a failing test and record the expected failure before implementation.
- Regenerate the tracked code graph after adding source or test files.

---

### Task 1: Connection-Local Retry Buffer Contract

**Files:**
- Create: `src/lib/net/SecureSocketWriteBuffer.h`
- Create: `src/lib/net/SecureSocketWriteBuffer.cpp`
- Create: `src/unittests/net/SecureSocketWriteBufferTests.cpp`
- Modify: `src/lib/net/CMakeLists.txt`
- Modify: `src/unittests/net/CMakeLists.txt`

**Interfaces:**
- Consumes: a realloc-compatible `void *(*)(void *, size_t)` function and caller-owned payload bytes.
- Produces: `SecureSocketWriteBuffer::capture(const void *, uint32_t)`, `data()`, `size()`, `retrying()`, `markRetry()`, and `clearRetry()`.

- [ ] **Step 1: Add the failing connection-isolation and allocation-failure tests**

Create a Qt guiless test whose essential cases are:

```cpp
#include "net/SecureSocketWriteBuffer.h"

#include <QTest>
#include <cstdlib>
#include <cstring>

namespace {
int g_reallocCalls = 0;

void *failSecondReallocation(void *current, size_t requested)
{
  ++g_reallocCalls;
  if (g_reallocCalls == 2) {
    return nullptr;
  }
  return std::realloc(current, requested);
}
}

class SecureSocketWriteBufferTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void keepsRetryPayloadConnectionLocal()
  {
    const char firstBytes[] = "first";
    const char secondBytes[] = "second";
    SecureSocketWriteBuffer first;
    SecureSocketWriteBuffer second;

    QVERIFY(first.capture(firstBytes, sizeof(firstBytes)));
    first.markRetry();
    QVERIFY(second.capture(secondBytes, sizeof(secondBytes)));

    QVERIFY(first.retrying());
    QVERIFY(!second.retrying());
    QCOMPARE(first.size(), uint32_t{sizeof(firstBytes)});
    QCOMPARE(second.size(), uint32_t{sizeof(secondBytes)});
    QCOMPARE(std::memcmp(first.data(), firstBytes, sizeof(firstBytes)), 0);
    QCOMPARE(std::memcmp(second.data(), secondBytes, sizeof(secondBytes)), 0);
  }

  void preservesPayloadWhenGrowthFails()
  {
    g_reallocCalls = 0;
    const char original[] = "old";
    const char larger[] = "larger payload";
    SecureSocketWriteBuffer buffer{failSecondReallocation};

    QVERIFY(buffer.capture(original, sizeof(original)));
    QVERIFY(!buffer.capture(larger, sizeof(larger)));

    QCOMPARE(buffer.size(), uint32_t{sizeof(original)});
    QCOMPARE(std::memcmp(buffer.data(), original, sizeof(original)), 0);
    QCOMPARE(g_reallocCalls, 2);
  }

  void clearsOnlyTheCompletedConnection()
  {
    const char bytes[] = "payload";
    SecureSocketWriteBuffer first;
    SecureSocketWriteBuffer second;
    QVERIFY(first.capture(bytes, sizeof(bytes)));
    QVERIFY(second.capture(bytes, sizeof(bytes)));
    first.markRetry();
    second.markRetry();

    first.clearRetry();

    QVERIFY(!first.retrying());
    QVERIFY(second.retrying());
  }
};

QTEST_GUILESS_MAIN(SecureSocketWriteBufferTests)
#include "SecureSocketWriteBufferTests.moc"
```

Add a `create_test(NAME SecureSocketWriteBufferTests ...)` entry that links
`net`, `base`, `arch`, `mt`, and `io`.

- [ ] **Step 2: Run the test and verify RED**

Run from a Visual Studio developer environment:

```powershell
$ieumCmake = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $ieumCmake -S . -B build -DSKIP_BUILD_TESTS=ON
& $ieumCmake --build build --target SecureSocketWriteBufferTests --parallel
```

Expected: compilation fails because `net/SecureSocketWriteBuffer.h` and its
class do not exist. A missing Qt package or compiler environment is an
environment error and must be fixed before continuing; it is not the expected
RED result.

- [ ] **Step 3: Implement the minimal buffer**

Use the following public shape:

```cpp
class SecureSocketWriteBuffer final
{
public:
  using Reallocate = void *(*)(void *, size_t);

  explicit SecureSocketWriteBuffer(Reallocate reallocate = std::realloc) noexcept;
  SecureSocketWriteBuffer(const SecureSocketWriteBuffer &) = delete;
  SecureSocketWriteBuffer(SecureSocketWriteBuffer &&) = delete;
  ~SecureSocketWriteBuffer();

  SecureSocketWriteBuffer &operator=(const SecureSocketWriteBuffer &) = delete;
  SecureSocketWriteBuffer &operator=(SecureSocketWriteBuffer &&) = delete;

  bool capture(const void *bytes, uint32_t size) noexcept;
  const void *data() const noexcept;
  uint32_t size() const noexcept;
  bool retrying() const noexcept;
  void markRetry() noexcept;
  void clearRetry() noexcept;

private:
  Reallocate m_reallocate;
  void *m_data = nullptr;
  uint32_t m_size = 0;
  uint32_t m_capacity = 0;
  bool m_retrying = false;
};
```

`capture()` must allocate through a temporary pointer, update `m_data` and
`m_capacity` only after success, and copy/update `m_size` only after capacity
is available:

```cpp
void *grown = m_reallocate(m_data, size);
if (grown == nullptr) {
  return false;
}
m_data = grown;
m_capacity = size;
```

The destructor frees `m_data` with `std::free`. Document that injected
reallocators must return memory compatible with `std::free`.

- [ ] **Step 4: Run the targeted test and verify GREEN**

```powershell
& $ieumCmake --build build --target SecureSocketWriteBufferTests --parallel
& $ieumCmake --build build --target run_tests --parallel
```

If the aggregate target is intentionally unavailable in the configured build,
run:

```powershell
& $ieumCmake --build build --target SecureSocketWriteBufferTests --parallel
& $ieumCmake -E chdir build ctest -R '^SecureSocketWriteBufferTests$' --output-on-failure
```

Expected: all three buffer cases pass and the failure allocator is called
exactly twice.

- [ ] **Step 5: Commit the tested buffer**

```powershell
git add src/lib/net/SecureSocketWriteBuffer.h src/lib/net/SecureSocketWriteBuffer.cpp src/lib/net/CMakeLists.txt src/unittests/net/SecureSocketWriteBufferTests.cpp src/unittests/net/CMakeLists.txt
git commit -m "test(net): define connection-local TLS retry buffer"
```

---

### Task 2: Make `SecureSocket::doWrite()` Use the Per-Connection Buffer

**Files:**
- Modify: `src/lib/net/SecureSocket.h`
- Modify: `src/lib/net/SecureSocket.cpp`
- Test: `src/unittests/net/SecureSocketWriteBufferTests.cpp`

**Interfaces:**
- Consumes: `SecureSocketWriteBuffer` from Task 1 and `StreamBuffer::peek()` / `getSize()`.
- Produces: one `m_writeBuffer` member per `SecureSocket`; no function-static write or retry state.

- [ ] **Step 1: Confirm the Task 1 regressions are green before refactoring**

```powershell
& $ieumCmake -E chdir build ctest -R '^SecureSocketWriteBufferTests$' --output-on-failure
```

These are the tests that define the per-connection ownership, failure
preservation, and retry-clear behavior used by the socket refactor. Do not add
a second state implementation inside `SecureSocket`.

- [ ] **Step 2: Implement the write-path integration**

Add `SecureSocketWriteBuffer m_writeBuffer;` to `SecureSocket`. Replace all
four `doWrite()` function statics with this member:

```cpp
uint32_t bufferSize = 0;
if (m_writeBuffer.retrying()) {
  bufferSize = m_writeBuffer.size();
} else {
  bufferSize = m_outputBuffer.getSize();
  if (bufferSize != 0 && !m_writeBuffer.capture(m_outputBuffer.peek(bufferSize), bufferSize)) {
    LOG_ERR("failed to grow TLS write retry buffer");
    isFatal(true);
    return Break;
  }
}
```

Pass `m_writeBuffer.data()` and a checked `int` size to `secureWrite()`. On
success call `clearRetry()`. On retry call `markRetry()` and leave the saved
size and bytes untouched. Before the OpenSSL call, reject a size greater than
`std::numeric_limits<int>::max()` as fatal rather than narrowing it.

Change the `secureRead()` and `secureWrite()` local retry counters from
function statics to `int retry = 0;` so concurrent connections do not share
them.

- [ ] **Step 3: Verify targeted and net tests**

```powershell
& $ieumCmake --build build --target SecureSocketWriteBufferTests SecureUtilsTests FingerprintTests FingerprintDatabaseTests --parallel
& $ieumCmake -E chdir build ctest -R '^(SecureSocketWriteBufferTests|SecureUtilsTests|FingerprintTests|FingerprintDatabaseTests)$' --output-on-failure
```

Expected: all selected tests pass. Compile warnings are failures in the normal
warnings-as-errors configuration.

- [ ] **Step 4: Commit the socket integration**

```powershell
git add src/lib/net/SecureSocket.h src/lib/net/SecureSocket.cpp src/unittests/net/SecureSocketWriteBufferTests.cpp
git commit -m "fix(net): isolate TLS write retry state per socket"
```

---

### Task 3: Reset the WinSock Writable-Poll Hint

**Files:**
- Modify: `src/lib/arch/IArchNetwork.h`
- Modify: `src/lib/arch/win32/ArchNetworkWinsock.h`
- Modify: `src/lib/arch/win32/ArchNetworkWinsock.cpp`
- Create: `src/unittests/platform/ArchNetworkWinsockTests.cpp`
- Modify: `src/unittests/platform/CMakeLists.txt`

**Interfaces:**
- Consumes: `ArchSocket` and WinSock's `ArchSocketImpl::m_pollWrite` cache.
- Produces: `IArchNetwork::resetPollWriteOnSocket(ArchSocket)` and the Windows override.

- [ ] **Step 1: Write the failing WinSock poll-cache test**

Add a Windows-only Qt test:

```cpp
#include "arch/win32/ArchNetworkWinsock.h"

#include <QTest>

class ArchNetworkWinsockTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void requestsFreshWritableNotification()
  {
    ArchNetworkWinsock network;
    ArchSocketImpl socket{INVALID_SOCKET, 1, WSA_INVALID_EVENT, false};

    network.resetPollWriteOnSocket(&socket);

    QVERIFY(socket.m_pollWrite);
  }
};

QTEST_GUILESS_MAIN(ArchNetworkWinsockTests)
#include "ArchNetworkWinsockTests.moc"
```

Register it inside the existing `if(WIN32)` block with `DEPENDS arch` and
`LIBS base mt`.

- [ ] **Step 2: Run the Windows target and verify RED**

```powershell
& $ieumCmake --build build --target ArchNetworkWinsockTests --parallel
```

Expected: compilation fails because `resetPollWriteOnSocket` is not declared.

- [ ] **Step 3: Implement the architecture method**

Add a documented virtual method with a default no-op body to `IArchNetwork`:

```cpp
virtual void resetPollWriteOnSocket(ArchSocket)
{
}
```

Declare the Windows override and implement it as:

```cpp
void ArchNetworkWinsock::resetPollWriteOnSocket(ArchSocket socket)
{
  assert(socket != nullptr);
  socket->m_pollWrite = true;
}
```

The default method keeps Unix subclasses source-compatible because the Unix
poll implementation does not cache writability in the same way.

- [ ] **Step 4: Run the Windows test and verify GREEN**

```powershell
& $ieumCmake --build build --target ArchNetworkWinsockTests --parallel
& $ieumCmake -E chdir build ctest -R '^ArchNetworkWinsockTests$' --output-on-failure
```

Expected: the test changes `m_pollWrite` from false to true and passes.

- [ ] **Step 5: Commit the poll-reset primitive**

```powershell
git add src/lib/arch/IArchNetwork.h src/lib/arch/win32/ArchNetworkWinsock.h src/lib/arch/win32/ArchNetworkWinsock.cpp src/unittests/platform/ArchNetworkWinsockTests.cpp src/unittests/platform/CMakeLists.txt
git commit -m "fix(win): expose TLS writable poll reset"
```

---

### Task 4: Wire `SSL_ERROR_WANT_WRITE` to the Poll Reset

**Files:**
- Modify: `src/lib/net/SecureSocket.cpp`
- Test: `src/unittests/platform/ArchNetworkWinsockTests.cpp`

**Interfaces:**
- Consumes: `ARCH->resetPollWriteOnSocket(getSocket())` from Task 3.
- Produces: a fresh writable notification request before `setWritable(true)` on the OpenSSL retry path.

- [ ] **Step 1: Strengthen the poll-state regression fixture**

Add a second test that starts with `m_pollWrite` true, simulates the poller
consuming that hint by setting it false, invokes `resetPollWriteOnSocket`, and
asserts it becomes true again. This stays as a separate named regression for
the exact busy-loop transition even though Task 3 has already made the
primitive green.

```cpp
void reArmsHintAfterPollerConsumedIt()
{
  ArchNetworkWinsock network;
  ArchSocketImpl socket{INVALID_SOCKET, 1, WSA_INVALID_EVENT, true};
  socket.m_pollWrite = false;

  network.resetPollWriteOnSocket(&socket);

  QVERIFY(socket.m_pollWrite);
}
```

- [ ] **Step 2: Call the reset from the exact OpenSSL error branch**

In `SecureSocket::checkResult()`, make the `SSL_ERROR_WANT_WRITE` branch:

```cpp
case SSL_ERROR_WANT_WRITE:
  ARCH->resetPollWriteOnSocket(getSocket());
  setWritable(true);
  retry++;
  LOG_VERBOSE("want to write, error=%d, attempt=%d", errorCode, retry);
  break;
```

The reset must precede `setWritable(true)` so the next multiplexer job waits
for a fresh WinSock readiness event.

- [ ] **Step 3: Re-run all targeted tests**

```powershell
& $ieumCmake --build build --target SecureSocketWriteBufferTests ArchNetworkWinsockTests --parallel
& $ieumCmake -E chdir build ctest -R '^(SecureSocketWriteBufferTests|ArchNetworkWinsockTests)$' --output-on-failure
```

Expected: both targets pass.

- [ ] **Step 4: Commit the OpenSSL retry wiring**

```powershell
git add src/lib/net/SecureSocket.cpp src/unittests/platform/ArchNetworkWinsockTests.cpp
git commit -m "fix(net): rearm WinSock polling on TLS backpressure"
```

---

### Task 5: Full Verification, Code Graph, and Pull Request

**Files:**
- Modify: `docs/codegraph/README.md`
- Modify: `docs/codegraph/codegraph.json`
- Modify if graph edges change: `docs/codegraph/component-graph.mmd`
- Reference: `docs/audit/2026-08-19-github-issues.md`

**Interfaces:**
- Consumes: all implementation commits and generated source inventory.
- Produces: reproducible graph artifacts, native test evidence, and a PR that closes #6 only after the required CI succeeds.

- [ ] **Step 1: Run formatting and focused static checks**

```powershell
clang-format --dry-run --Werror src/lib/net/SecureSocketWriteBuffer.h src/lib/net/SecureSocketWriteBuffer.cpp src/lib/net/SecureSocket.h src/lib/net/SecureSocket.cpp src/lib/arch/IArchNetwork.h src/lib/arch/win32/ArchNetworkWinsock.h src/lib/arch/win32/ArchNetworkWinsock.cpp src/unittests/net/SecureSocketWriteBufferTests.cpp src/unittests/platform/ArchNetworkWinsockTests.cpp
git diff --check
```

Use the repository-pinned clang-format binary if `clang-format` is not on
`PATH`; do not accept a different major version's rewrite without review.

- [ ] **Step 2: Run the native Windows build and tests**

```powershell
& $ieumCmake --build build --parallel
& $ieumCmake -E chdir build ctest --output-on-failure
```

Expected: all build targets pass. The known interactive clipboard host denial
may be recorded as environment-limited only if the same direct Win32 access
probe reproduces it; every new socket or poll test must pass.

- [ ] **Step 3: Regenerate and verify the code graph**

```powershell
$env:PYTHONDONTWRITEBYTECODE = '1'
uv run --no-project --python 3.13 python tools/generate-codegraph.py --root . --output docs/codegraph
uv run --no-project --python 3.13 python -m unittest discover -s tools/tests -p test_codegraph.py -v
uvx ruff check tools/codegraph.py tools/generate-codegraph.py tools/tests/test_codegraph.py
```

Run the generator twice and require byte-identical outputs on the second run.

- [ ] **Step 4: Run privacy and license gates before publication**

Run the tracked privacy regression suite, then run both tree and
`origin/ieum/main..HEAD` scans with the owner-private deny map and frozen
digest. Run REUSE on a clean exported Linux tree so Windows symlink
materialization does not create false failures.

Expected: privacy result `verified-clean`, zero findings, and REUSE compliant.

- [ ] **Step 5: Commit generated artifacts**

```powershell
git add docs/codegraph/README.md docs/codegraph/codegraph.json docs/codegraph/component-graph.mmd
git commit -m "docs: refresh code graph for TLS write reliability"
```

- [ ] **Step 6: Push, open the PR, and merge through protection**

Push the feature branch normally. The PR body must include:

```markdown
Fixes #6

- makes TLS retry bytes and flags connection-local
- preserves the previous buffer on allocation failure
- rearms the WinSock writable-poll hint on `SSL_ERROR_WANT_WRITE`

Tests:
- SecureSocketWriteBufferTests
- ArchNetworkWinsockTests
- full native build and CTest
- privacy tree/range scans
```

Wait for required `ci-passed`, verify the PR head SHA has not changed, merge
through the protected `ieum/main` branch, and read back issue #6. The issue is
complete only when the merged PR and successful native checks are visible.
