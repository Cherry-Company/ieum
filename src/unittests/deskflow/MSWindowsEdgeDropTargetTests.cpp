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
    return ++m_refs;
  }
  ULONG STDMETHODCALLTYPE Release() override
  {
    return --m_refs;
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

private:
  ULONG m_refs = 1;
};

std::vector<std::filesystem::path> paths()
{
  return {
      std::filesystem::current_path() / L"한글 file.txt",
      std::filesystem::current_path() / L"second.bin",
  };
}

void implementsComIdentity()
{
  auto *target = new MSWindowsEdgeDropTarget({.direction = Direction::Left});
  require(target->QueryInterface(IID_IDropTarget, nullptr) == E_POINTER, "null interface output should fail");
  void *object = nullptr;
  require(target->QueryInterface(IID_IDropTarget, &object) == S_OK, "drop target interface should resolve");
  require(static_cast<IDropTarget *>(object)->Release() == 1, "queried reference should release");
  require(target->Release() == 0, "owner reference should delete target");
}

void handsOffPathsAfterDwellWithCopySemantics()
{
  FakeDataObject dataObject;
  auto now = EdgeHandoffDecision::TimePoint{};
  std::size_t loadCalls = 0;
  std::size_t handoffCalls = 0;
  POINTL handedPoint{};
  std::vector<std::filesystem::path> handedPaths;
  MSWindowsEdgeDropCallbacks callbacks;
  callbacks.direction = Direction::Left;
  callbacks.handoff = [&](POINTL point, std::vector<std::filesystem::path> value) {
    ++handoffCalls;
    handedPoint = point;
    handedPaths = std::move(value);
  };
  auto loader = [&](IDataObject *object) -> std::optional<std::vector<std::filesystem::path>> {
    require(object == &dataObject, "loader should receive the live data object only during calls");
    ++loadCalls;
    return paths();
  };
  auto *target = new MSWindowsEdgeDropTarget(callbacks, {}, [&] { return now; }, loader);

  DWORD effect = DROPEFFECT_MOVE;
  require(target->DragEnter(&dataObject, 0, POINTL{0, 240}, &effect) == S_OK, "path drag should enter");
  require(effect == DROPEFFECT_NONE && target->pathCount() == 2, "drag should wait without opening files");
  now += 250ms;
  effect = DROPEFFECT_MOVE;
  require(target->DragOver(0, POINTL{0, 240}, &effect) == S_OK, "dwell should arm");
  require(effect == DROPEFFECT_COPY, "armed edge must advertise copy even when the source proposed move");

  effect = DROPEFFECT_MOVE;
  require(target->Drop(&dataObject, 0, POINTL{0, 240}, &effect) == S_OK, "armed paths should drop");
  require(effect == DROPEFFECT_COPY && handoffCalls == 1, "drop must hand off once as copy");
  require(handedPoint.x == 0 && handedPoint.y == 240, "handoff should retain desktop point");
  require(handedPaths == paths(), "handoff should retain only paths");
  require(loadCalls == 2, "drop should re-read the bounded path list before handoff");
  require(target->state() == EdgeHandoffState::Idle && target->pathCount() == 0, "drop should clear state");
  require(target->Release() == 0, "target should release cleanly");
}

void rejectsChangedOrMissingPaths()
{
  FakeDataObject dataObject;
  auto now = EdgeHandoffDecision::TimePoint{};
  std::size_t calls = 0;
  std::size_t handoffs = 0;
  MSWindowsEdgeDropCallbacks callbacks{
      .direction = Direction::Right,
      .handoff = [&](POINTL, auto) { ++handoffs; },
  };
  auto loader = [&](IDataObject *) -> std::optional<std::vector<std::filesystem::path>> {
    ++calls;
    if (calls == 1) {
      return paths();
    }
    return std::vector{std::filesystem::current_path() / "changed.txt"};
  };
  auto *target = new MSWindowsEdgeDropTarget(callbacks, {}, [&] { return now; }, loader);
  DWORD effect = DROPEFFECT_COPY;
  target->DragEnter(&dataObject, 0, POINTL{}, &effect);
  now += 250ms;
  target->DragOver(0, POINTL{}, &effect);
  target->Drop(&dataObject, 0, POINTL{}, &effect);
  require(effect == DROPEFFECT_NONE && handoffs == 0, "changed path list should fail closed");
  require(target->DragLeave() == S_OK, "drag leave should clear safely");
  require(target->Release() == 0, "target should release cleanly");
}

} // namespace

int main()
{
  implementsComIdentity();
  handsOffPathsAfterDwellWithCopySemantics();
  rejectsChangedOrMissingPaths();
  std::cout << "PASS: MSWindowsEdgeDropTargetTests\n";
  return 0;
}
