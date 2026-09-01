# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT

include_guard(GLOBAL)

# Windows Installer compares only three numeric fields, while PE version
# resources expose four. Map both to the same monotonically increasing build
# slot so an alpha upgrade cannot treat the new runtime as the old file.
function(ieum_compute_windows_versions release_version installer_version_out file_version_out)
  if(release_version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)-(alpha|beta|rc)\\.([0-9]+)$")
    set(major "${CMAKE_MATCH_1}")
    set(minor "${CMAKE_MATCH_2}")
    set(patch "${CMAKE_MATCH_3}")
    set(channel "${CMAKE_MATCH_4}")
    set(sequence "${CMAKE_MATCH_5}")
    if(sequence GREATER 199)
      message(FATAL_ERROR "Windows prerelease sequence exceeds 199: ${release_version}")
    endif()

    if(channel STREQUAL "alpha")
      set(channel_offset 100)
    elseif(channel STREQUAL "beta")
      set(channel_offset 400)
    else()
      set(channel_offset 700)
    endif()
    math(EXPR build "${patch} * 1000 + ${channel_offset} + ${sequence}")
  elseif(release_version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    set(major "${CMAKE_MATCH_1}")
    set(minor "${CMAKE_MATCH_2}")
    set(patch "${CMAKE_MATCH_3}")
    math(EXPR build "${patch} * 1000 + 999")
  elseif(release_version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    set(major "${CMAKE_MATCH_1}")
    set(minor "${CMAKE_MATCH_2}")
    set(patch "${CMAKE_MATCH_3}")
    set(sequence "${CMAKE_MATCH_4}")
    if(sequence GREATER 98)
      set(sequence 98)
    endif()
    math(EXPR build "${patch} * 1000 + 900 + ${sequence}")
  else()
    message(FATAL_ERROR "Cannot map release version to Windows versions: ${release_version}")
  endif()

  if(major GREATER 255 OR minor GREATER 255 OR build GREATER 65535)
    message(FATAL_ERROR "Windows version is out of range: ${release_version}")
  endif()

  set(${installer_version_out} "${major}.${minor}.${build}" PARENT_SCOPE)
  set(${file_version_out} "${major}.${minor}.${build}.0" PARENT_SCOPE)
endfunction()
