# SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2014 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Check if variable MAC_INSTALLER_BACKGROUND_FILE is defined.
# Make sure that the MAC_INSTALLER_BACKGROUND_FILE contains the full path, ie.
# includes CMAKE_SOURCE_DIR or so.

if (DEFINED MAC_INSTALLER_BACKGROUND_FILE )
  set(MAC_INSTALLER_DO_CUSTOM_BACKGROUND "1")
else()
  set(MAC_INSTALLER_DO_CUSTOM_BACKGROUND "0")
endif()

find_package(Qt${QT_VERSION_MAJOR} ${REQUIRED_QT_VERSION} COMPONENTS Core REQUIRED)

# Set debug entitlements conditionally based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG_ENTITLEMENTS "\t<key>com.apple.security.get-task-allow</key>\n\t<true/>")
else()
    set(DEBUG_ENTITLEMENTS "")
endif()

# Sandbox migration description
configure_file(container-migration.plist.cmake ${CMAKE_CURRENT_BINARY_DIR}/container-migration.plist @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/admin/osx/container-migration.plist
        DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources)

configure_file(create_mac.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/create_mac.sh)
configure_file(macosx.entitlements.cmake ${CMAKE_CURRENT_BINARY_DIR}/macosx.entitlements)
configure_file(macosx.pkgproj.cmake ${CMAKE_CURRENT_BINARY_DIR}/macosx.pkgproj)
configure_file(pre_install.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/pre_install.sh)
configure_file(post_install.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/post_install.sh)

# Generate CFBundleLocalizations for MacOSXBundleInfo.plist.in
file(GLOB_RECURSE TS_FILES RELATIVE ${CMAKE_SOURCE_DIR} "${CMAKE_SOURCE_DIR}/translations/client_*.ts")
set(LOCALIZATIONS_LIST "")
foreach(TS_FILE ${TS_FILES})
    get_filename_component(LANG_CODE ${TS_FILE} NAME_WE) # client_en_GB
    string(REPLACE "client_" "" LANG_CODE ${LANG_CODE}) # en_GB
    string(REPLACE "_" "-" LANG_CODE ${LANG_CODE}) # en-GB
    set(LOCALIZATIONS_LIST "${LOCALIZATIONS_LIST}        <string>${LANG_CODE}</string>\n")
endforeach()
if(LOCALIZATIONS_LIST)
    string(STRIP "${LOCALIZATIONS_LIST}" LOCALIZATIONS_LIST)
endif()
set(MACOSX_BUNDLE_LOCALIZATIONS "${LOCALIZATIONS_LIST}" CACHE INTERNAL "Generated list of localizations for macOS bundle" FORCE)
