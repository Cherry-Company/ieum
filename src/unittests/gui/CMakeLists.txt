# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

add_subdirectory(config)
add_subdirectory(core)

create_test(
  NAME LoggerTests
  DEPENDS gui
  SOURCE LoggerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME LogWidgetTests
  DEPENDS gui
  SOURCE LogWidgetTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME FileTailTests
  DEPENDS gui
  LIBS base
  SOURCE FileTailTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME KeySequenceTests
  DEPENDS gui
  SOURCE KeySequenceTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME IpcClientTests
  DEPENDS gui
  SOURCE IpcClientTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ServiceStartCoordinatorTests
  DEPENDS gui
  SOURCE ServiceStartCoordinatorTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME CoreActionPolicyTests
  DEPENDS gui
  SOURCE CoreActionPolicyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME MacAccessibilityGuideTests
  DEPENDS gui
  SOURCE MacAccessibilityGuideTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME MacApplicationLifecyclePolicyTests
  DEPENDS gui
  SOURCE MacApplicationLifecyclePolicyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME UpdateShutdownTests
  DEPENDS gui
  SOURCE UpdateShutdownTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ServerConfigDialogTests
  DEPENDS gui
  SOURCE ServerConfigDialogTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(ServerConfigDialogTests PRIVATE "${PROJECT_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME ScreenSetupModelTests
  DEPENDS gui
  SOURCE ScreenSetupModelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME StyleUtilsTests
  DEPENDS gui
  SOURCE StyleUtilsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME StatusBarTests
  DEPENDS gui
  LIBS base common
  SOURCE StatusBarTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME VersionCheckerTests
  DEPENDS gui
  SOURCE VersionCheckerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME DiagnosticTests
  DEPENDS gui
  SOURCE DiagnosticTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ProLicenseUiPolicyTests
  DEPENDS gui licensing
  SOURCE ProLicenseUiPolicyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
