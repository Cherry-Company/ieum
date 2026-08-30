/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferTypes.h"

namespace deskflow::filetransfer {

[[nodiscard]] OfferValidation validateTransferRoute(
    const TransferId &id, const std::string &sourceScreen, const std::string &targetScreen,
    const FileTransferLimits &limits = {}
);

[[nodiscard]] OfferValidation validateOffer(const FileTransferOffer &offer, const FileTransferLimits &limits = {});

} // namespace deskflow::filetransfer
