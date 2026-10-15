# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

add_subdirectory(config)
add_subdirectory(core)

add_executable(CoreProcessTestHelper CoreProcessTestHelper.cpp)
target_link_libraries(CoreProcessTestHelper PRIVATE Qt::Core)
set_target_properties(CoreProcessTestHelper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

create_test(
  NAME CoreProcessTests
  DEPENDS gui
  SOURCE CoreProcessTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
add_dependencies(CoreProcessTests CoreProcessTestHelper)

create_test(
  NAME ConnectionFlowTests
  DEPENDS gui
  SOURCE ConnectionFlowTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
add_dependencies(ConnectionFlowTests CoreProcessTestHelper)
target_sources(ConnectionFlowTests PRIVATE "${CMAKE_SOURCE_DIR}/src/apps/res/deskflow.qrc")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND BUILD_X11_SUPPORT)
  # Run through tools/tests/run_host_switch.py, which owns three isolated X
  # displays and TLS-enabled cores. It is separate from the portable unit suite.
  add_executable(HostSwitchIntegrationTests HostSwitchIntegrationTests.cpp)
  target_link_libraries(HostSwitchIntegrationTests PRIVATE gui Qt::Test)
  target_sources(HostSwitchIntegrationTests PRIVATE "${CMAKE_SOURCE_DIR}/src/apps/res/deskflow.qrc")
  set_target_properties(HostSwitchIntegrationTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

create_test(
  NAME MessagesTests
  DEPENDS gui
  SOURCE MessagesTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME LoggerTests
  DEPENDS gui
  SOURCE LoggerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ExternalUrlLauncherTests
  DEPENDS gui
  SOURCE ExternalUrlLauncherTests.cpp
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

if(WIN32)
  create_test(
    NAME WindowsFileTransferDropBrokerTests
    DEPENDS gui
    SOURCE WindowsFileTransferDropBrokerTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
  )
endif()

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
