# SPDX-FileCopyrightText: (C) 2024 - 2025 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-License-Identifier: MIT

set(CORE_BINARY "${IEUM_CORE_TARGET}")
if(WIN32)
  string(APPEND CORE_BINARY ".exe")
endif()

configure_file(Constants.h.in Constants.h @ONLY)
configure_file(VersionInfo.h.in VersionInfo.h @ONLY)

add_library(common STATIC
  Coordinate.h
  Enums.h
  ExitCodes.h
  FullscreenGeometry.h
  I18N.h
  I18N.cpp
  LogLevel.h
  NetworkInterfaces.cpp
  NetworkInterfaces.h
  NetworkProtocol.h
  PlatformInfo.h
  Settings.h
  Settings.cpp
  TailscaleIntegration.cpp
  TailscaleIntegration.h
  QSettingsProxy.cpp
  QSettingsProxy.h
  UrlConstants.h
  ${CMAKE_CURRENT_BINARY_DIR}/Constants.h
  ${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.h
)

target_link_libraries(common PUBLIC Qt6::Core Qt6::Network)

target_include_directories(common
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/lib>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/lib>
)
