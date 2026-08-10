/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "base/Log.h"
#include "deskflow/ProtocolUtil.h"
#include "server/Server.h"

#include <cstring>

ClientProxy1_9::ClientProxy1_9(const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events)
    : ClientProxy1_8(name, stream, server, events)
{
  // The first status is requested when this screen becomes active.
}

void ClientProxy1_9::inputLanguageControl(deskflow::InputLanguageAction action, const std::string &target)
{
  LOG_DEBUG(
      "send input language control to \"%s\": action=%d target=\"%s\"", getName().c_str(), static_cast<int>(action),
      target.c_str()
  );
  ProtocolUtil::writef(getStream(), kMsgDInputLangControl, static_cast<int8_t>(action), &target);
}

deskflow::InputLanguageStatus ClientProxy1_9::inputLanguageStatus() const
{
  return m_inputLanguageStatus;
}

bool ClientProxy1_9::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgCInputLangStatus, 4) != 0) {
    return ClientProxy1_8::parseMessage(code);
  }

  std::string sourceId;
  int8_t category = static_cast<int8_t>(deskflow::InputLanguageCategory::Unknown);
  int8_t composing = 0;
  ProtocolUtil::readf(getStream(), kMsgCInputLangStatus + 4, &sourceId, &category, &composing);

  if (category < static_cast<int8_t>(deskflow::InputLanguageCategory::KeyLayout) ||
      category > static_cast<int8_t>(deskflow::InputLanguageCategory::Unknown)) {
    LOG_WARN("client \"%s\" sent invalid input language category %d", getName().c_str(), category);
    category = static_cast<int8_t>(deskflow::InputLanguageCategory::Unknown);
  }

  m_inputLanguageStatus = {sourceId, static_cast<deskflow::InputLanguageCategory>(category), composing != 0};
  LOG_DEBUG(
      "input language status from \"%s\": source=\"%s\" category=%d composing=%d", getName().c_str(), sourceId.c_str(),
      category, composing
  );
  getServer()->onInputLanguageStatus(this, m_inputLanguageStatus);
  return true;
}
