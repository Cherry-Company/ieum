/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/EdgeHandoffDecision.h"
#include "platform/MSWindowsFileDropExtractor.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <objidl.h>
#include <oleidl.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

namespace deskflow::filetransfer {

struct MSWindowsEdgeDropCallbacks
{
  using HandoffHandler = std::function<void(POINTL, std::vector<std::filesystem::path>)>;
  using StateHandler = std::function<void(EdgeHandoffState, std::size_t)>;

  Direction direction = Direction::NoDirection;
  HandoffHandler handoff;
  StateHandler stateChanged;
};

struct MSWindowsEdgeDropOptions
{
  std::chrono::milliseconds dwell{250};
  WindowsFileDropLimits dropLimits;
};

class MSWindowsEdgeDropTarget final : public IDropTarget
{
public:
  using Clock = std::function<EdgeHandoffDecision::TimePoint()>;
  using PathLoader = std::function<std::optional<std::vector<std::filesystem::path>>(IDataObject *)>;

  explicit MSWindowsEdgeDropTarget(
      MSWindowsEdgeDropCallbacks callbacks, MSWindowsEdgeDropOptions options = {}, Clock clock = {},
      PathLoader pathLoader = {}
  );

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void **object) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *dataObject, DWORD keyState, POINTL point, DWORD *effect) override;
  HRESULT STDMETHODCALLTYPE DragOver(DWORD keyState, POINTL point, DWORD *effect) override;
  HRESULT STDMETHODCALLTYPE DragLeave() override;
  HRESULT STDMETHODCALLTYPE Drop(IDataObject *dataObject, DWORD keyState, POINTL point, DWORD *effect) override;

  [[nodiscard]] EdgeHandoffState state() const noexcept;
  [[nodiscard]] std::size_t pathCount() const noexcept;

private:
  ~MSWindowsEdgeDropTarget() = default;

  [[nodiscard]] std::optional<std::vector<std::filesystem::path>> loadPaths(IDataObject *dataObject) const;
  [[nodiscard]] EdgeHandoffDecision::TimePoint now() const;
  void observe();
  void setEffect(DWORD *effect) const noexcept;
  void clearSession(bool publish);
  void publishState() const noexcept;

  volatile LONG m_refCount = 1;
  MSWindowsEdgeDropCallbacks m_callbacks;
  MSWindowsEdgeDropOptions m_options;
  Clock m_clock;
  PathLoader m_pathLoader;
  EdgeHandoffDecision m_decision;
  std::vector<std::filesystem::path> m_paths;
  bool m_dragActive = false;
};

} // namespace deskflow::filetransfer
