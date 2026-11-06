# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

find_package(OpenSSL ${REQUIRED_OPENSSL_VERSION} REQUIRED COMPONENTS Crypto)

create_test(
  NAME PlanLicenseTests
  DEPENDS licensing
  LIBS OpenSSL::Crypto
  SOURCE PlanLicenseTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/licensing"
)

create_test(
  NAME ProLicenseTests
  DEPENDS licensing
  SOURCE ProLicenseTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/licensing"
)

create_test(
  NAME ProEarlyAccessClaimTests
  DEPENDS licensing
  SOURCE ProEarlyAccessClaimTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/licensing"
)
