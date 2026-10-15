# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT

create_test(
  NAME ArchTimeTests
  DEPENDS arch
  LIBS base mt
  SOURCE ArchTimeTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/arch"
)
if(WIN32)
  target_link_libraries(ArchTimeTests version)
endif()
