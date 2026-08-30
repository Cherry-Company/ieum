/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/win32/MSWindowsEdgeDropTarget.h"

#include "platform/MSWindowsFileTransferSource.h"

#include <algorithm>
#include <utility>

namespace deskflow::filetransfer {

MSWindowsEdgeDropTarget::MSWindowsEdgeDropTarget(
    MSWindowsEdgeDropCallbacks callbacks, MSWindowsEdgeDropOptions options, Clock clock, SourceLoader sourceLoader
)
    : m_callbacks(std::move(callbacks)),
      m_options(std::move(options)),
      m_clock(std::move(clock)),
      m_sourceLoader(std::move(sourceLoader)),
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

HRESULT STDMETHODCALLTYPE
MSWindowsEdgeDropTarget::DragEnter(IDataObject *dataObject, DWORD, POINTL point, DWORD *effect)
{
  if (effect == nullptr) {
    return E_POINTER;
  }

  const auto allowedEffects = *effect;
  clearSession(false);

  try {
    auto loaded = loadSources(dataObject);
    if (loaded.has_value()) {
      m_sources = std::move(loaded->sources);
      m_totalBytes = loaded->totalBytes;
      m_dragActive = true;
      observe(point);
    } else {
      publishState();
    }
  } catch (...) {
    clearSession(true);
  }

  setEffect(allowedEffects, effect);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MSWindowsEdgeDropTarget::DragOver(DWORD, POINTL point, DWORD *effect)
{
  if (effect == nullptr) {
    return E_POINTER;
  }

  const auto allowedEffects = *effect;
  try {
    if (m_dragActive) {
      observe(point);
    }
  } catch (...) {
    clearSession(true);
  }

  setEffect(allowedEffects, effect);
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

  const auto allowedEffects = *effect;
  *effect = DROPEFFECT_NONE;

  try {
    if (!m_dragActive) {
      clearSession(true);
      return S_OK;
    }

    observe(point);
    const auto copyAllowed = (allowedEffects & DROPEFFECT_COPY) != 0;
    if (!copyAllowed || m_decision.state() != EdgeHandoffState::Armed || !m_callbacks.handoff) {
      clearSession(true);
      return S_OK;
    }

    auto fresh = loadSources(dataObject);
    if (!fresh.has_value() || fresh->sources != m_sources) {
      clearSession(true);
      return S_OK;
    }

    auto releasedTarget = m_decision.release();
    if (!releasedTarget.has_value()) {
      clearSession(true);
      return S_OK;
    }

    auto verifiedSources = std::move(fresh->sources);
    m_sources.clear();
    m_totalBytes = 0;
    m_dragActive = false;
    m_callbacks.handoff(*releasedTarget, std::move(verifiedSources));
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

std::size_t MSWindowsEdgeDropTarget::sourceCount() const noexcept
{
  return m_sources.size();
}

std::optional<MSWindowsEdgeDropTarget::LoadedSources>
MSWindowsEdgeDropTarget::loadSources(IDataObject *dataObject) const
{
  if (dataObject == nullptr) {
    return std::nullopt;
  }

  auto sources = m_sourceLoader ? m_sourceLoader(dataObject) : loadNativeSources(dataObject);
  if (!sources.has_value()) {
    return std::nullopt;
  }

  TransferId validationId{};
  validationId[0] = 1;
  auto manifest = buildFileTransferSourceManifest(
      validationId, "local-source", "local-target", std::move(*sources), m_options.transferLimits
  );
  if (!manifest.ok()) {
    return std::nullopt;
  }

  return LoadedSources{
      .sources = std::move(manifest.manifest.sources),
      .totalBytes = manifest.validation.totalBytes,
  };
}

std::optional<std::vector<FileTransferSourceCandidate>>
MSWindowsEdgeDropTarget::loadNativeSources(IDataObject *dataObject) const
{
  auto extraction = extractWindowsFileDropDataObject(dataObject, m_options.dropLimits);
  if (!extraction.ok()) {
    return std::nullopt;
  }

  const auto maxItems = (std::min)(m_options.dropLimits.maxItems, m_options.transferLimits.maxItems);
  auto inspection = inspectMSWindowsFileTransferSources(extraction.paths, maxItems);
  if (!inspection.ok()) {
    return std::nullopt;
  }
  return std::move(inspection.sources);
}

std::optional<EdgeTarget> MSWindowsEdgeDropTarget::resolveTarget(POINTL point) const
{
  return m_callbacks.resolveTarget ? m_callbacks.resolveTarget(point) : std::nullopt;
}

EdgeHandoffDecision::TimePoint MSWindowsEdgeDropTarget::now() const
{
  return m_clock ? m_clock() : EdgeHandoffDecision::Clock::now();
}

void MSWindowsEdgeDropTarget::observe(POINTL point)
{
  (void)m_decision.observe(resolveTarget(point), now());
  publishState();
}

void MSWindowsEdgeDropTarget::setEffect(DWORD allowedEffects, DWORD *effect) const noexcept
{
  const auto canCopy = (allowedEffects & DROPEFFECT_COPY) != 0;
  *effect =
      m_dragActive && canCopy && m_decision.state() == EdgeHandoffState::Armed ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

void MSWindowsEdgeDropTarget::clearSession(bool publish)
{
  m_decision.cancel();
  m_sources.clear();
  m_totalBytes = 0;
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
    m_callbacks.stateChanged(m_decision.state(), m_decision.target(), m_sources.size(), m_totalBytes);
  } catch (...) {
    // UI observation must not escape through the COM drag-and-drop boundary.
  }
}

} // namespace deskflow::filetransfer
