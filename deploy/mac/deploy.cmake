# SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-License-Identifier: MIT

# HACK This is set when the files is included so its the real path
# calling CMAKE_CURRENT_LIST_DIR after include would return the wrong scope var
set(MY_DIR ${CMAKE_CURRENT_LIST_DIR})
set(OSX_BUNDLE ${BUILD_OSX_BUNDLE})

set(OS_STRING "macos-${BUILD_ARCHITECTURE}")

if (OSX_BUNDLE)
  set(
    APPLE_CODESIGN_DISTRIBUTION
    ""
    CACHE STRING
    "Developer ID Application identity used to sign distributable macOS packages"
  )
  mark_as_advanced(APPLE_CODESIGN_DISTRIBUTION)

  if(APPLE_CODESIGN_DISTRIBUTION)
    set(
      IEUM_MACDEPLOYQT_SIGN_ARGUMENT
      "-sign-for-notarization=${APPLE_CODESIGN_DISTRIBUTION}"
    )
  else()
    # Keep development packages internally consistent even when a trusted
    # Developer ID is not available. Gatekeeper still requires user approval.
    set(IEUM_MACDEPLOYQT_SIGN_ARGUMENT "-codesign=-")
  endif()

  # Run deployment and signing after CPack has staged every installed file.
  # Signing from install(CODE) is too early: later LICENSE installs invalidate
  # the bundle resource seal and macOS reports that the app is damaged.
  configure_file(
    ${MY_DIR}/pre-cpack.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pre-cpack-macos.cmake
    @ONLY
  )
  set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_CURRENT_BINARY_DIR}/pre-cpack-macos.cmake)

  set(CPACK_PACKAGE_ICON "${MY_DIR}/ieum-volume.icns")
  set(CPACK_DMG_BACKGROUND_IMAGE "${MY_DIR}/dmg-background.tiff")
  set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${MY_DIR}/generate_ds_store.applescript")
  set(CPACK_DMG_VOLUME_NAME "${CMAKE_PROJECT_PROPER_NAME}")
  set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
  set(CPACK_GENERATOR "DragNDrop")
endif()
