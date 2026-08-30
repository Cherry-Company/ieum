/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/win32/MSWindowsEdgeDropTarget.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <objidl.h>
#include <oleidl.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using namespace deskflow::filetransfer;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

class FakeDataObject final : public IDataObject
{
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void **object) override
  {
    if (object == nullptr) {
      return E_POINTER;
    }
    if (IsEqualIID(interfaceId, IID_IUnknown) || IsEqualIID(interfaceId, IID_IDataObject)) {
      *object = static_cast<IDataObject *>(this);
      AddRef();
      return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override
  {
    ++m_addRefCalls;
    return ++m_refCount;
  }

  ULONG STDMETHODCALLTYPE Release() override
  {
    ++m_releaseCalls;
    return --m_refCount;
  }

  HRESULT STDMETHODCALLTYPE GetData(FORMATETC *, STGMEDIUM *) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *, STGMEDIUM *) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC *, FORMATETC *) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetData(FORMATETC *, STGMEDIUM *, BOOL) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC **) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *, DWORD, IAdviseSink *, DWORD *) override
  {
    return OLE_E_ADVISENOTSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override
  {
    return OLE_E_ADVISENOTSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **) override
  {
    return OLE_E_ADVISENOTSUPPORTED;
  }

  [[nodiscard]] ULONG addRefCalls() const noexcept
  {
    return m_addRefCalls;
  }

  [[nodiscard]] ULONG releaseCalls() const noexcept
  {
    return m_releaseCalls;
  }

private:
  ULONG m_refCount = 1;
  ULONG m_addRefCalls = 0;
  ULONG m_releaseCalls = 0;
};

FileTransferSourceCandidate candidate(std::uint64_t size = 7, std::uint64_t lastWriteTime = 1234)
{
  FileTransferSourceCandidate value;
  value.path = std::filesystem::current_path() / "source" / "item.bin";
  value.identity.length = 2;
  value.identity.bytes[0] = 0x42;
  value.identity.bytes[1] = 0x24;
  value.size = size;
  value.lastWriteTime = lastWriteTime;
  value.name = "한글 문서.txt";
  return value;
}

std::optional<EdgeTarget> resolveRightEdge(POINTL point)
{
  if (point.x <= 5) {
    return EdgeTarget{Direction::Right, "living-room"};
  }
  return std::nullopt;
}

void implementsComIdentityAndReferenceCounting()
{
  auto *target = new MSWindowsEdgeDropTarget({.resolveTarget = resolveRightEdge, .handoff = [](auto &, auto) {}});

  require(target->QueryInterface(IID_IDropTarget, nullptr) == E_POINTER, "null QueryInterface output should fail");

  void *unsupported = target;
  require(target->QueryInterface(IID_IDataObject, &unsupported) == E_NOINTERFACE, "unsupported interface should fail");
  require(unsupported == nullptr, "unsupported interface output should be null");

  void *dropTarget = nullptr;
  require(target->QueryInterface(IID_IDropTarget, &dropTarget) == S_OK, "IDropTarget query should succeed");
  require(dropTarget == static_cast<IDropTarget *>(target), "IDropTarget query should return this object");
  require(
      static_cast<IDropTarget *>(dropTarget)->Release() == 1, "queried reference should release to owner reference"
  );
  require(target->Release() == 0, "owner reference should delete the target");
}

void armsAtDwellAndEmitsOneCopyHandoff()
{
  FakeDataObject dataObject;
  auto now = EdgeHandoffDecision::TimePoint{};
  std::size_t loadCalls = 0;
  std::size_t handoffCalls = 0;
  EdgeTarget handedTarget;
  std::vector<FileTransferSourceCandidate> handedSources;
  std::vector<EdgeHandoffState> states;
  std::vector<std::uint64_t> totals;

  MSWindowsEdgeDropCallbacks callbacks;
  callbacks.resolveTarget = resolveRightEdge;
  callbacks.handoff = [&](const EdgeTarget &target, std::vector<FileTransferSourceCandidate> sources) {
    ++handoffCalls;
    handedTarget = target;
    handedSources = std::move(sources);
  };
  callbacks.stateChanged = [&](EdgeHandoffState state, const auto &, std::size_t, std::uint64_t totalBytes) {
    states.push_back(state);
    totals.push_back(totalBytes);
  };

  auto loader = [&](IDataObject *object) -> std::optional<std::vector<FileTransferSourceCandidate>> {
    require(object == &dataObject, "loader should receive the current COM object only during the call");
    ++loadCalls;
    return std::vector{candidate()};
  };
  auto *target = new MSWindowsEdgeDropTarget(callbacks, {}, [&] { return now; }, loader);

  DWORD effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
  require(target->DragEnter(&dataObject, 0, POINTL{0, 10}, &effect) == S_OK, "valid DragEnter should succeed");
  require(effect == DROPEFFECT_NONE, "candidate should not advertise copy before dwell");
  require(target->state() == EdgeHandoffState::Candidate, "DragEnter should start candidate state");
  require(target->sourceCount() == 1, "DragEnter should retain only one source snapshot");

  now += 249ms;
  effect = DROPEFFECT_COPY;
  require(target->DragOver(0, POINTL{0, 10}, &effect) == S_OK, "DragOver before dwell should succeed");
  require(effect == DROPEFFECT_NONE, "249 ms should remain unarmed");

  now += 1ms;
  effect = DROPEFFECT_COPY;
  require(target->DragOver(0, POINTL{0, 10}, &effect) == S_OK, "DragOver at dwell should succeed");
  require(effect == DROPEFFECT_COPY, "250 ms should advertise copy");
  require(target->state() == EdgeHandoffState::Armed, "250 ms should arm the handoff");

  effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
  require(target->Drop(&dataObject, 0, POINTL{0, 10}, &effect) == S_OK, "armed Drop should succeed");
  require(effect == DROPEFFECT_COPY, "armed Drop should finish as a copy");
  require(loadCalls == 2, "Drop should re-snapshot the data once before accepting");
  require(handoffCalls == 1, "armed Drop should emit exactly one handoff");
  require(handedTarget == EdgeTarget{Direction::Right, "living-room"}, "handoff should retain the resolved target");
  require(handedSources == std::vector{candidate()}, "handoff should retain the verified source snapshot");
  require(target->state() == EdgeHandoffState::Idle, "Drop should reset state");
  require(target->sourceCount() == 0, "Drop should clear local snapshots after callback");
  require(dataObject.addRefCalls() == 0 && dataObject.releaseCalls() == 0, "target must not retain the IDataObject");
  require(!states.empty() && states.back() == EdgeHandoffState::Idle, "state callback should finish at Idle");
  require(totals.size() >= 3 && totals[0] == 7, "state callback should publish the checked total");
  require(target->Release() == 0, "target should release cleanly");
}

void movingAwayAndDragLeaveCancel()
{
  FakeDataObject dataObject;
  auto now = EdgeHandoffDecision::TimePoint{};
  std::size_t handoffCalls = 0;
  MSWindowsEdgeDropCallbacks callbacks;
  callbacks.resolveTarget = resolveRightEdge;
  callbacks.handoff = [&](const auto &, auto) { ++handoffCalls; };
  auto loader = [](IDataObject *) -> std::optional<std::vector<FileTransferSourceCandidate>> {
    return std::vector{candidate()};
  };
  auto *target = new MSWindowsEdgeDropTarget(callbacks, {}, [&] { return now; }, loader);

  DWORD effect = DROPEFFECT_COPY;
  target->DragEnter(&dataObject, 0, POINTL{0, 0}, &effect);
  now += 250ms;
  effect = DROPEFFECT_COPY;
  target->DragOver(0, POINTL{0, 0}, &effect);
  require(target->state() == EdgeHandoffState::Armed, "setup should arm");

  effect = DROPEFFECT_COPY;
  target->DragOver(0, POINTL{100, 0}, &effect);
  require(effect == DROPEFFECT_NONE, "moving away should remove copy effect");
  require(target->state() == EdgeHandoffState::Idle, "moving away should disarm");

  effect = DROPEFFECT_COPY;
  target->Drop(&dataObject, 0, POINTL{100, 0}, &effect);
  require(effect == DROPEFFECT_NONE && handoffCalls == 0, "drop away from edge should do nothing");

  now += 1ms;
  effect = DROPEFFECT_COPY;
  target->DragEnter(&dataObject, 0, POINTL{0, 0}, &effect);
  require(target->DragLeave() == S_OK, "DragLeave should succeed");
  require(
      target->state() == EdgeHandoffState::Idle && target->sourceCount() == 0, "DragLeave should clear the session"
  );
  require(target->Release() == 0, "target should release cleanly");
}

void rejectsChangedUnsafeAndNonCopyDrops()
{
  FakeDataObject dataObject;
  auto now = EdgeHandoffDecision::TimePoint{};
  std::size_t loadCalls = 0;
  std::size_t handoffCalls = 0;
  MSWindowsEdgeDropCallbacks callbacks;
  callbacks.resolveTarget = resolveRightEdge;
  callbacks.handoff = [&](const auto &, auto) { ++handoffCalls; };
  auto changedLoader = [&](IDataObject *) -> std::optional<std::vector<FileTransferSourceCandidate>> {
    ++loadCalls;
    return std::vector{candidate(loadCalls == 1 ? 7 : 8)};
  };
  auto *target = new MSWindowsEdgeDropTarget(callbacks, {}, [&] { return now; }, changedLoader);

  DWORD effect = DROPEFFECT_COPY;
  target->DragEnter(&dataObject, 0, POINTL{0, 0}, &effect);
  now += 250ms;
  effect = DROPEFFECT_COPY;
  target->DragOver(0, POINTL{0, 0}, &effect);
  effect = DROPEFFECT_COPY;
  target->Drop(&dataObject, 0, POINTL{0, 0}, &effect);
  require(effect == DROPEFFECT_NONE, "changed source snapshot should reject Drop");
  require(handoffCalls == 0, "changed source should not emit handoff");

  auto unsafe = candidate();
  unsafe.name = "../unsafe.txt";
  auto unsafeLoader = [unsafe](IDataObject *) -> std::optional<std::vector<FileTransferSourceCandidate>> {
    return std::vector{unsafe};
  };
  target->Release();
  target = new MSWindowsEdgeDropTarget(callbacks, {}, [&] { return now; }, unsafeLoader);
  effect = DROPEFFECT_COPY;
  target->DragEnter(&dataObject, 0, POINTL{0, 0}, &effect);
  require(effect == DROPEFFECT_NONE && target->sourceCount() == 0, "unsafe candidate should be rejected at DragEnter");

  auto validLoader = [](IDataObject *) -> std::optional<std::vector<FileTransferSourceCandidate>> {
    return std::vector{candidate()};
  };
  target->Release();
  target = new MSWindowsEdgeDropTarget(callbacks, {}, [&] { return now; }, validLoader);
  effect = DROPEFFECT_MOVE;
  target->DragEnter(&dataObject, 0, POINTL{0, 0}, &effect);
  now += 250ms;
  effect = DROPEFFECT_MOVE;
  target->DragOver(0, POINTL{0, 0}, &effect);
  require(effect == DROPEFFECT_NONE, "move-only source should never advertise copy");
  effect = DROPEFFECT_MOVE;
  target->Drop(&dataObject, 0, POINTL{0, 0}, &effect);
  require(effect == DROPEFFECT_NONE && handoffCalls == 0, "move-only Drop should not emit handoff");

  require(target->DragEnter(&dataObject, 0, POINTL{}, nullptr) == E_POINTER, "null effect should fail safely");
  require(target->Release() == 0, "target should release cleanly");
}

} // namespace

int main()
{
  implementsComIdentityAndReferenceCounting();
  armsAtDwellAndEmitsOneCopyHandoff();
  movingAwayAndDragLeaveCancel();
  rejectsChangedUnsafeAndNonCopyDrops();
  std::cout << "PASS: MSWindowsEdgeDropTargetTests\n";
  return 0;
}
