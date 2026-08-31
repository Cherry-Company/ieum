# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

create_test(
  NAME ProLicenseTests
  DEPENDS licensing
  SOURCE ProLicenseTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/licensing"
)
