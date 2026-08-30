/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/EdgeHandoffDecision.h"
#include "deskflow/FileTransferSourceManifest.h"
#include "platform/MSWindowsFileDropExtractor.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <objidl.h>
#include <oleidl.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace deskflow::filetransfer {

struct MSWindowsEdgeDropCallbacks
{
  using TargetResolver = std::function<std::optional<EdgeTarget>(POINTL)>;
  using HandoffHandler = std::function<void(const EdgeTarget &, std::vector<FileTransferSourceCandidate>)>;
  using StateHandler =
      std::function<void(EdgeHandoffState, const std::optional<EdgeTarget> &, std::size_t, std::uint64_t)>;

  TargetResolver resolveTarget;
  HandoffHandler handoff;
  StateHandler stateChanged;
};

struct MSWindowsEdgeDropOptions
{
  std::chrono::milliseconds dwell{250};
  FileTransferLimits transferLimits;
  WindowsFileDropLimits dropLimits;
};

class MSWindowsEdgeDropTarget final : public IDropTarget
{
public:
  using Clock = std::function<EdgeHandoffDecision::TimePoint()>;
  using SourceLoader = std::function<std::optional<std::vector<FileTransferSourceCandidate>>(IDataObject *)>;

  explicit MSWindowsEdgeDropTarget(
      MSWindowsEdgeDropCallbacks callbacks, MSWindowsEdgeDropOptions options = {}, Clock clock = {},
      SourceLoader sourceLoader = {}
  );

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void **object) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *dataObject, DWORD keyState, POINTL point, DWORD *effect) override;
  HRESULT STDMETHODCALLTYPE DragOver(DWORD keyState, POINTL point, DWORD *effect) override;
  HRESULT STDMETHODCALLTYPE DragLeave() override;
  HRESULT STDMETHODCALLTYPE Drop(IDataObject *dataObject, DWORD keyState, POINTL point, DWORD *effect) override;

  [[nodiscard]] EdgeHandoffState state() const noexcept;
  [[nodiscard]] std::size_t sourceCount() const noexcept;

private:
  struct LoadedSources
  {
    std::vector<FileTransferSourceCandidate> sources;
    std::uint64_t totalBytes = 0;
  };

  ~MSWindowsEdgeDropTarget() = default;

  [[nodiscard]] std::optional<LoadedSources> loadSources(IDataObject *dataObject) const;
  [[nodiscard]] std::optional<std::vector<FileTransferSourceCandidate>>
  loadNativeSources(IDataObject *dataObject) const;
  [[nodiscard]] std::optional<EdgeTarget> resolveTarget(POINTL point) const;
  [[nodiscard]] EdgeHandoffDecision::TimePoint now() const;

  void observe(POINTL point);
  void setEffect(DWORD allowedEffects, DWORD *effect) const noexcept;
  void clearSession(bool publish);
  void publishState() const noexcept;

  volatile LONG m_refCount = 1;
  MSWindowsEdgeDropCallbacks m_callbacks;
  MSWindowsEdgeDropOptions m_options;
  Clock m_clock;
  SourceLoader m_sourceLoader;
  EdgeHandoffDecision m_decision;
  std::vector<FileTransferSourceCandidate> m_sources;
  std::uint64_t m_totalBytes = 0;
  bool m_dragActive = false;
};

} // namespace deskflow::filetransfer
