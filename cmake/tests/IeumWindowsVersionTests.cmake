# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT

include("${CMAKE_CURRENT_LIST_DIR}/../IeumWindowsVersion.cmake")

function(assert_windows_versions release_version expected_installer expected_file)
  ieum_compute_windows_versions(
    "${release_version}"
    actual_installer
    actual_file
  )
  if(NOT actual_installer STREQUAL expected_installer)
    message(FATAL_ERROR
      "${release_version}: installer version '${actual_installer}' != '${expected_installer}'"
    )
  endif()
  if(NOT actual_file STREQUAL expected_file)
    message(FATAL_ERROR
      "${release_version}: file version '${actual_file}' != '${expected_file}'"
    )
  endif()
endfunction()

assert_windows_versions("0.1.0-alpha.23" "0.1.123" "0.1.123.0")
assert_windows_versions("0.1.0-alpha.24" "0.1.124" "0.1.124.0")
assert_windows_versions("2.3.4-beta.5" "2.3.4405" "2.3.4405.0")
assert_windows_versions("2.3.4-rc.7" "2.3.4707" "2.3.4707.0")
assert_windows_versions("2.3.4" "2.3.4999" "2.3.4999.0")
assert_windows_versions("2.3.4.8" "2.3.4908" "2.3.4908.0")

message(STATUS "Ieum Windows version mapping tests passed")
