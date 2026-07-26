# SPDX-FileCopyrightText: (C) 2024 - 2025 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-FileCopyrightText: (C) 2024 Symless Ltd
# SPDX-License-Identifier: MIT

set(target ${IEUM_GUI_TARGET})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(WIN32)
  set(EXE_DESCRIPTION "${CMAKE_PROJECT_PROPER_NAME} GUI configuration tool")
  set(EXE_ICON "IDI_ICON1 ICON DISCARDABLE \"${CMAKE_SOURCE_DIR}/src/apps/res/ieum.ico\" ")
  configure_file(${CMAKE_SOURCE_DIR}/src/apps/res/windows.rc.in deskflow.rc)
  set(platform_extra deskflow.rc)
elseif(BUILD_OSX_BUNDLE)
  #setup our bundle plist file
  set(BUNDLE_EXECUTABLE_NAME "${target}")
  set(BUNDLE_BUNDLE_NAME "${target}")
  set(BUNDLE_DISPLAY_NAME "${target}")
  set(BUNDLE_GUI_IDENTIFIER "${CMAKE_PROJECT_MACOS_BUNDLE_ID}")
  set(BUNDLE_ICON_FILE Ieum.icns)
  set(BUNDLE_INFO_STRING "${CMAKE_PROJECT_DESCRIPTION}")
  set(BUNDLE_COPYRIGHT "${CMAKE_PROJECT_COPYRIGHT}")

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../res/deskflow.plist.in
    ${CMAKE_CURRENT_BINARY_DIR}/deskflow.plist
    @ONLY
  )

  set(bundle_icon ../res/Ieum.icns)
  set(bundle_localizations ../res/macos/ko.lproj/InfoPlist.strings)
  set_source_files_properties(${bundle_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  set_source_files_properties(${bundle_localizations} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/ko.lproj")
  set(platform_extra ${bundle_icon} ${bundle_localizations})
endif()

add_executable(${target} WIN32 MACOSX_BUNDLE
  ${platform_extra}
  ../res/deskflow.qrc
  deskflow-gui.cpp
)

target_link_libraries(
  ${target}
  gui
  common
  Qt6::Core
  Qt6::Widgets
  Qt6::Network)

install(
  TARGETS ${target}
  RUNTIME_DEPENDENCY_SET guiDeps
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  BUNDLE DESTINATION .
)

if(WIN32)
  set_target_properties(${target} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:LIBCMT")

  install(RUNTIME_DEPENDENCY_SET guiDeps
    ${WIN32_RUNTIME_DEPENDENCY_SET_ARGS}
    PRE_EXCLUDE_REGEXES ${WIN32_PRE_EXCLUDE_REGEXES}
    POST_EXCLUDE_REGEXES ${WIN32_POST_EXCLUDE_REGEXES}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

  set(QT_DEPENDS_DIR ${CMAKE_BINARY_DIR}/qt-depends)

  add_custom_command(
    TARGET ${target} POST_BUILD
    COMMAND ${DEPLOYQT}
      --no-compiler-runtime
      --no-system-d3d-compiler
      --no-opengl-sw
      --no-quick-import
      --dir "${QT_DEPENDS_DIR}"
      --plugindir "${QT_DEPENDS_DIR}/plugins"
      $<TARGET_FILE:${target}>
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
      ${QT_DEPENDS_DIR}
      $<TARGET_FILE_DIR:${target}>
  )

  install(
    DIRECTORY ${QT_DEPENDS_DIR}/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PATTERN "dx*.dll" EXCLUDE
    PATTERN "icu*.dll" EXCLUDE
  )

elseif(APPLE)
  if (BUILD_OSX_BUNDLE)
    set_target_properties(${target} PROPERTIES
      INSTALL_RPATH "@loader_path/../Libraries;@loader_path/../Frameworks"
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/deskflow.plist"
    )
    if (APPLE_CODESIGN_DEV)
      configure_mac_codesign(${target})
    endif()
  else()
    set_target_properties(${target} PROPERTIES MACOSX_BUNDLE FALSE)
  endif()
else()
  generate_app_man(${target} "${CMAKE_PROJECT_DESCRIPTION} \\(GUI\\)")
endif()
