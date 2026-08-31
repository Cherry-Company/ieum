# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

if (WIN32)
  create_test(
    NAME ArchNetworkWinsockTests
    DEPENDS arch
    LIBS base mt version
    SOURCE ArchNetworkWinsockTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/arch"
  )
  create_test(
    NAME MSWindowsMouseInputTests
    DEPENDS platform
    LIBS base arch
    SOURCE MSWindowsMouseInputTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME MSWindowsClipboardTests
    DEPENDS platform
    LIBS base arch
    SOURCE MSWindowsClipboardTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME MSWindowsFileDropExtractorTests
    DEPENDS platform
    LIBS base arch
    SOURCE MSWindowsFileDropExtractorTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME MSWindowsFileTransferSourceTests
    DEPENDS app
    LIBS base arch
    SOURCE MSWindowsFileTransferSourceTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME MSWindowsFileTransferIoTests
    DEPENDS app
    LIBS platform base arch
    SOURCE MSWindowsFileTransferIoTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
elseif(APPLE)
  create_test(
    NAME OSXClipboardTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXClipboardTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME OSXKeyStateTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXKeyStateTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
elseif(UNIX)
  if (BUILD_X11_SUPPORT)
    create_test(
      NAME XWindowsClipboardTests
      DEPENDS platform
      LIBS base arch
      SOURCE XWindowsClipboardTests.cpp
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
    )
  endif()
endif()
