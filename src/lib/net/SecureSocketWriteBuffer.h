/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

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
