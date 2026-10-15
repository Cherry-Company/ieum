# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "The resource probe currently measures Linux /proc and getrusage only")
endif()

add_executable(ieum-resource-probe ResourceProbe.cpp)
target_include_directories(ieum-resource-probe PRIVATE "${PROJECT_SOURCE_DIR}/src/lib" "${PROJECT_BINARY_DIR}/src/lib")
target_link_libraries(ieum-resource-probe PRIVATE gui app)
