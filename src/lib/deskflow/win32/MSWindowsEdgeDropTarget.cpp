/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/win32/MSWindowsEdgeDropTarget.h"

#include <utility>

namespace deskflow::filetransfer {

MSWindowsEdgeDropTarget::MSWindowsEdgeDropTarget(
    MSWindowsEdgeDropCallbacks callbacks, MSWindowsEdgeDropOptions options, Clock clock, PathLoader pathLoader
)
    : m_callbacks(std::move(callbacks)),
      m_options(std::move(options)),
      m_clock(std::move(clock)),
      m_pathLoader(std::move(pathLoader)),
      m_decision(m_options.dwell)
{
}

HRESULT STDMETHODCALLTYPE MSWindowsEdgeDropTarget::QueryInterface(REFIID interfaceId, void **object)
{
  if (object == nullptr) {
    return E_POINTER;
  }
  if (IsEqualIID(interfaceId, IID_IUnknown) || IsEqualIID(interfaceId, IID_IDropTarget)) {
    *object = static_cast<IDropTarget *>(this);
    AddRef();
    return S_OK;
  }
  *object = nullptr;
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MSWindowsEdgeDropTarget::AddRef()
{
  return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

ULONG STDMETHODCALLTYPE MSWindowsEdgeDropTarget::Release()
{
  const auto references = InterlockedDecrement(&m_refCount);
  if (references == 0) {
    delete this;
    return 0;
  }
  return static_cast<ULONG>(references);
}

HRESULT STDMETHODCALLTYPE MSWindowsEdgeDropTarget::DragEnter(IDataObject *dataObject, DWORD, POINTL, DWORD *effect)
{
  if (effect == nullptr) {
    return E_POINTER;
  }
  clearSession(false);
  try {
    auto paths = loadPaths(dataObject);
    if (paths.has_value()) {
      m_paths = std::move(*paths);
      m_dragActive = true;
      observe();
    } else {
      publishState();
    }
  } catch (...) {
    clearSession(true);
  }
  setEffect(effect);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MSWindowsEdgeDropTarget::DragOver(DWORD, POINTL, DWORD *effect)
{
  if (effect == nullptr) {
    return E_POINTER;
  }
  try {
    if (m_dragActive) {
      observe();
    }
  } catch (...) {
    clearSession(true);
  }
  setEffect(effect);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MSWindowsEdgeDropTarget::DragLeave()
{
  clearSession(true);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MSWindowsEdgeDropTarget::Drop(IDataObject *dataObject, DWORD, POINTL point, DWORD *effect)
{
  if (effect == nullptr) {
    return E_POINTER;
  }
  *effect = DROPEFFECT_NONE;
  try {
    if (!m_dragActive) {
      clearSession(true);
      return S_OK;
    }
    observe();
    if (m_decision.state() != EdgeHandoffState::Armed || !m_callbacks.handoff) {
      clearSession(true);
      return S_OK;
    }
    auto fresh = loadPaths(dataObject);
    if (!fresh.has_value() || *fresh != m_paths || !m_decision.release().has_value()) {
      clearSession(true);
      return S_OK;
    }

    auto paths = std::move(*fresh);
    m_paths.clear();
    m_dragActive = false;
    m_callbacks.handoff(point, std::move(paths));
    *effect = DROPEFFECT_COPY;
    publishState();
  } catch (...) {
    clearSession(true);
    *effect = DROPEFFECT_NONE;
  }
  return S_OK;
}

EdgeHandoffState MSWindowsEdgeDropTarget::state() const noexcept
{
  return m_decision.state();
}

std::size_t MSWindowsEdgeDropTarget::pathCount() const noexcept
{
  return m_paths.size();
}

std::optional<std::vector<std::filesystem::path>> MSWindowsEdgeDropTarget::loadPaths(IDataObject *dataObject) const
{
  if (dataObject == nullptr) {
    return std::nullopt;
  }
  if (m_pathLoader) {
    return m_pathLoader(dataObject);
  }
  auto extraction = extractWindowsFileDropDataObject(dataObject, m_options.dropLimits);
  if (!extraction.ok()) {
    return std::nullopt;
  }
  return std::move(extraction.paths);
}

EdgeHandoffDecision::TimePoint MSWindowsEdgeDropTarget::now() const
{
  return m_clock ? m_clock() : EdgeHandoffDecision::Clock::now();
}

void MSWindowsEdgeDropTarget::observe()
{
  (void)m_decision.observe(EdgeTarget{.direction = m_callbacks.direction, .screen = "edge"}, now());
  publishState();
}

void MSWindowsEdgeDropTarget::setEffect(DWORD *effect) const noexcept
{
  *effect = m_dragActive && m_decision.state() == EdgeHandoffState::Armed ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

void MSWindowsEdgeDropTarget::clearSession(bool publish)
{
  m_decision.cancel();
  m_paths.clear();
  m_dragActive = false;
  if (publish) {
    publishState();
  }
}

void MSWindowsEdgeDropTarget::publishState() const noexcept
{
  if (!m_callbacks.stateChanged) {
    return;
  }
  try {
    m_callbacks.stateChanged(m_decision.state(), m_paths.size());
  } catch (...) {
  }
}

} // namespace deskflow::filetransfer
