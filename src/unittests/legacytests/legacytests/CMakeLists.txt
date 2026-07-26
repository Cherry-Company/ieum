# SPDX-FileCopyrightText: (C) 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: (C) 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

config_test()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(target legacytests)
add_executable(${target} ${sources} ${headers})
target_link_libraries(${target} ${test_libs} common)

if(WIN32)
  # Both targets deploy Qt DLLs into build/bin. Serialize them to avoid
  # vcpkg applocal reading a DLL while windeployqt is replacing it.
  add_dependencies(${target} ${IEUM_GUI_TARGET})
endif()
