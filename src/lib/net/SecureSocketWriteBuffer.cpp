/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/SecureSocketWriteBuffer.h"

#include <cstring>

SecureSocketWriteBuffer::SecureSocketWriteBuffer(Reallocate reallocate) noexcept : m_reallocate{reallocate}
{
}

SecureSocketWriteBuffer::~SecureSocketWriteBuffer()
{
  std::free(m_data);
}

bool SecureSocketWriteBuffer::capture(const void *bytes, uint32_t size) noexcept
{
  if (size != 0 && bytes == nullptr) {
    return false;
  }

  if (size > m_capacity) {
    void *grown = m_reallocate(m_data, size);
    if (grown == nullptr) {
      return false;
    }
    m_data = grown;
    m_capacity = size;
  }

  if (size != 0) {
    std::memcpy(m_data, bytes, size);
  }
  m_size = size;
  return true;
}

const void *SecureSocketWriteBuffer::data() const noexcept
{
  return m_data;
}

uint32_t SecureSocketWriteBuffer::size() const noexcept
{
  return m_size;
}

bool SecureSocketWriteBuffer::retrying() const noexcept
{
  return m_retrying;
}

void SecureSocketWriteBuffer::markRetry() noexcept
{
  m_retrying = true;
}

void SecureSocketWriteBuffer::clearRetry() noexcept
{
  m_retrying = false;
}
