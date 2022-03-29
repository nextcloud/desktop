if(APPLE)
  set(OC_OEM_SHARE_ICNS "${CMAKE_BINARY_DIR}/src/gui/${APPLICATION_ICON_NAME}.icns")

  # The bundle identifier and application group need to have compatible values with the client
  # to be able to open a Mach port across the extension's sandbox boundary.
  # Pass the info through the xcodebuild command line and make sure that the project uses
  # those user-defined settings to build the plist.
  add_custom_target( mac_overlayplugin ALL
    xcodebuild ARCHS=${CMAKE_OSX_ARCHITECTURES} ONLY_ACTIVE_ARCH=NO
    -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/OwnCloudFinderSync/OwnCloudFinderSync.xcodeproj
    -target FinderSyncExt -configuration Release "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
    "OC_OEM_SHARE_ICNS=${OC_OEM_SHARE_ICNS}"
    "OC_APPLICATION_NAME=${APPLICATION_NAME}"
    "OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}"
    "OC_SOCKETAPI_TEAM_IDENTIFIER_PREFIX=${SOCKETAPI_TEAM_IDENTIFIER_PREFIX}"
    COMMENT building Mac Overlay icons
    VERBATIM)

  add_custom_target( mac_fileproviderplugin ALL
    xcodebuild ARCHS=${CMAKE_OSX_ARCHITECTURES} ONLY_ACTIVE_ARCH=NO
    -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/OwnCloudFinderSync/OwnCloudFinderSync.xcodeproj
    -target FileProviderExt -configuration Release "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
    "OC_APPLICATION_NAME=${APPLICATION_NAME}"
    "OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}"
    "OC_SOCKETAPI_TEAM_IDENTIFIER_PREFIX=${SOCKETAPI_TEAM_IDENTIFIER_PREFIX}"
    COMMENT building macOS File Provider extension
    VERBATIM)

  add_dependencies(mac_overlayplugin mac_fileproviderplugin nextcloud) # for the ownCloud.icns to be generated

  if (BUILD_OWNCLOUD_OSX_BUNDLE)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Release/FinderSyncExt.appex
      DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns
      USE_SOURCE_PERMISSIONS)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Release/FileProviderExt.appex
      DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns
      USE_SOURCE_PERMISSIONS)
  endif()
endif()

