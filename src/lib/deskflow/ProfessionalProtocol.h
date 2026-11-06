// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#pragma once
#include "common/ProfessionalMessage.h"
namespace deskflow {
class IStream;
namespace professional {
bool writeFrame(IStream *stream, const Message &message);
std::optional<Message> readFrame(IStream *stream);
} // namespace professional
} // namespace deskflow
