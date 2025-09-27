# SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2012 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
include( InstallRequiredSystemLibraries )

set( CPACK_PACKAGE_CONTACT  "Dominik Schmidt <domme@tomahawk-player.org>" )

include("${CMAKE_SOURCE_DIR}/NEXTCLOUD.cmake")

include( VERSION.cmake )
set( CPACK_PACKAGE_VERSION_MAJOR  ${MIRALL_VERSION_MAJOR} )
set( CPACK_PACKAGE_VERSION_MINOR  ${MIRALL_VERSION_MINOR} )
set( CPACK_PACKAGE_VERSION_PATCH  ${MIRALL_VERSION_PATCH} )
set( CPACK_PACKAGE_VERSION_BUILD  ${MIRALL_VERSION_BUILD} )
set( CPACK_PACKAGE_VERSION  ${MIRALL_VERSION_FULL}${MIRALL_VERSION_SUFFIX} )

if(APPLE)
    set( CPACK_GENERATOR "DragNDrop" )
    set( CPACK_SOURCE_GENERATOR "")
    set( CPACK_PACKAGE_FILE_NAME ${APPLICATION_SHORTNAME}-${CPACK_PACKAGE_VERSION} )
    set( CPACK_PACKAGE_ICON ${CMAKE_BINARY_DIR}/src/gui/${APPLICATION_ICON_NAME}.icns)

    set( CPACK_DMG_DS_STORE "${CMAKE_SOURCE_DIR}/admin/osx/DS_Store.in")
#    set( CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/admin/osx/DMGBackground.png" )

    set( CPACK_DMG_FORMAT "UDBZ" )
    set( CPACK_DMG_VOLUME_NAME "${APPLICATION_SHORTNAME}")

    # did not work with cmake 2.8.7, so we override MacOSXBundleInfo.plist.in
    #set( CPACK_BUNDLE_PLIST ${CMAKE_SOURCE_DIR}/admin/osx/Info.plist )

    # do we need these?
    #set( CPACK_SYSTEM_NAME "OSX" )
    #set( CPACK_PACKAGE_NAME "FOO" )
    #set( CPACK_BUNDLE_NAME "BAR" )
endif()


set( CPACK_TOPLEVEL_TAG "unused" ) # Directory for the installed files.  - needed to provide anything to avoid an error# CPACK_INSTALL_COMMANDS  Extra commands to install components.


# Set the options file that needs to be included inside CMakeCPackOptions.cmake
configure_file("${CMAKE_SOURCE_DIR}/CPackOptions.cmake.in"
    "${CMAKE_BINARY_DIR}/CPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackOptions.cmake") # File included at cpack time, once per generator after setting CPACK_GENERATOR to the actual generator being used; allows per-generator setting of CPACK_* variables at cpack time.  ${PROJECT_BINARY_DIR}/CPackOptions.cmake
include(CPack)
