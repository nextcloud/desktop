
if(APPLE)
add_custom_target( legacy_mac_overlayplugin ALL
    xcodebuild -workspace ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/OwnCloud.xcworkspace
        -scheme SyncStateFinder.osax -configuration Release SYMROOT=${CMAKE_CURRENT_BINARY_DIR}
        OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}
        OC_SOCKETAPI_TEAM_IDENTIFIER_PREFIX=${SOCKETAPI_TEAM_IDENTIFIER_PREFIX}
    COMMENT building Legacy Mac Overlay icons)

# The bundle identifier and application group need to have compatible values with the client
# to be able to open a Mach port across the extension's sandbox boundary.
# Pass the info through the xcodebuild command line and make sure that the project uses
# those user-defined settings to build the plist.
add_custom_target( mac_overlayplugin ALL
    xcodebuild -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/OwnCloudFinderSync/OwnCloudFinderSync.xcodeproj
        -target FinderSyncExt -configuration Release SYMROOT=${CMAKE_CURRENT_BINARY_DIR}
        OC_APPLICATION_NAME=${APPLICATION_NAME}
        OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}
        OC_SOCKETAPI_TEAM_IDENTIFIER_PREFIX=${SOCKETAPI_TEAM_IDENTIFIER_PREFIX}
    COMMENT building Mac Overlay icons)

INSTALL( DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Release/SyncStateFinder.osax/Contents
         DESTINATION ${CMAKE_INSTALL_PREFIX}/Library/ScriptingAdditions/SyncStateFinder.osax/ )
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Release/FinderSyncExt.appex
    DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Plugins
    USE_SOURCE_PERMISSIONS)
endif(APPLE)

