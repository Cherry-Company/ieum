# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

find_package(OpenSSL ${REQUIRED_OPENSSL_VERSION} REQUIRED COMPONENTS Crypto)

add_library(licensing STATIC
  ProLicense.cpp
  ProLicense.h
  ProLicensePublicKey.h
)

target_link_libraries(licensing
  PUBLIC Qt6::Core
  PRIVATE OpenSSL::Crypto
)

target_include_directories(licensing
  PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/lib>
)
