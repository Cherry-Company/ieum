/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_10.h"

ClientProxy1_10::ClientProxy1_10(
    const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events
)
    : ClientProxy1_9(name, stream, server, events)
{
}
