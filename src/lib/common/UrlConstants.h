/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

// important: this is used for settings paths on some platforms,
// and must not be a url. qt automatically converts this to reverse domain
// notation (rdn), e.g. org.deskflow
const auto kOrgDomain = QStringLiteral("deskflow.org");

const auto kUrlApp = QStringLiteral("https://github.com/Cherry-Company/ieum");
const auto kUrlHelp = QStringLiteral("https://github.com/Cherry-Company/ieum/issues");
const auto kUrlDownload = QStringLiteral("https://github.com/Cherry-Company/ieum/releases");
const auto kUrlSponsor = QStringLiteral("https://github.com/sponsors/victoriousian");
const auto kUrlProEarlyAccessSponsor = QStringLiteral("https://github.com/sponsors/victoriousian/sponsorships");
const auto kProEarlyAccessEmail = QStringLiteral("easecompany@protonmail.ch");
const auto kUrlUpdateCheck = QStringLiteral("https://raw.githubusercontent.com/Cherry-Company/ieum/ieum/main/VERSION");

#if defined(Q_OS_LINUX)
const auto kUrlGnomeTrayFix = QStringLiteral("https://extensions.gnome.org/extension/615/appindicator-support/");
#endif
