if(APPLE)
set(OC_OEM_SHARE_ICNS "${CMAKE_BINARY_DIR}/src/gui/${APPLICATION_ICON_NAME}.icns")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(XCODE_CONFIG "Debug")
else()
    set(XCODE_CONFIG "Release")
endif()

# The bundle identifier and application group need to have compatible values with the client
# to be able to open a Mach port across the extension's sandbox boundary.
# Pass the info through the xcodebuild command line and make sure that the project uses
# those user-defined settings to build the plist.
add_custom_target( mac_overlayplugin ALL
    xcodebuild -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/OwnCloudFinderSync/OwnCloudFinderSync.xcodeproj
        -target FinderSyncExt -configuration "${XCODE_CONFIG}" "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
        "OC_OEM_SHARE_ICNS=${OC_OEM_SHARE_ICNS}"
        "OC_APPLICATION_NAME=${APPLICATION_NAME}"
        "OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}"
        "OC_SOCKETAPI_TEAM_IDENTIFIER_PREFIX=${SOCKETAPI_TEAM_IDENTIFIER_PREFIX}"
    COMMENT building Mac Overlay icons
    VERBATIM)
add_dependencies(mac_overlayplugin owncloud) # for the ownCloud.icns to be generated
add_custom_command(TARGET mac_overlayplugin POST_BUILD COMMAND ${CMAKE_COMMAND}
    ARGS -E copy_directory "${CMAKE_CURRENT_BINARY_DIR}/${XCODE_CONFIG}/FinderSyncExt.appex" "$<TARGET_FILE_DIR:owncloud>/../PlugIns/FinderSyncExt.appex" MAIN_DEPENDENCY "${mac_overlayplugin}")

endif()

