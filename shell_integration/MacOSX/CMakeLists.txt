if(APPLE)
    set(OC_OEM_SHARE_ICNS "${CMAKE_BINARY_DIR}/src/gui/${APPLICATION_ICON_NAME}.icns")

    if (CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
        set(XCODE_TARGET_CONFIGURATION "Debug")
    else()
        set(XCODE_TARGET_CONFIGURATION "Release")
    endif()

    # The bundle identifier and application group need to have compatible values with the client
    # to be able to open a Mach port across the extension's sandbox boundary.
    # Pass the info through the xcodebuild command line and make sure that the project uses
    # those user-defined settings to build the plist.
    add_custom_target( mac_overlayplugin ALL
        xcodebuild ARCHS=${CMAKE_OSX_ARCHITECTURES} ONLY_ACTIVE_ARCH=NO
        -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj
        -target FinderSyncExt -configuration ${XCODE_TARGET_CONFIGURATION} "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
        "OC_OEM_SHARE_ICNS=${OC_OEM_SHARE_ICNS}"
        "OC_APPLICATION_NAME=${APPLICATION_NAME}"
        "OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}"
        "OC_SOCKETAPI_TEAM_IDENTIFIER_PREFIX=${SOCKETAPI_TEAM_IDENTIFIER_PREFIX}"
        COMMENT building Mac Overlay icons
        VERBATIM)

    if (BUILD_FILE_PROVIDER_MODULE)
        add_custom_target( mac_fileproviderplugin ALL
            xcodebuild ARCHS=${CMAKE_OSX_ARCHITECTURES} ONLY_ACTIVE_ARCH=NO
            -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj
            -target FileProviderExt -configuration ${XCODE_TARGET_CONFIGURATION} "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
            "OC_APPLICATION_EXECUTABLE_NAME=${APPLICATION_EXECUTABLE}"
            "OC_APPLICATION_VENDOR=${APPLICATION_VENDOR}"
            "OC_APPLICATION_NAME=${APPLICATION_NAME}"
            "OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}"
            "OC_SOCKETAPI_TEAM_IDENTIFIER_PREFIX=${SOCKETAPI_TEAM_IDENTIFIER_PREFIX}"
            COMMENT building macOS File Provider extension
            VERBATIM)

        add_dependencies(mac_overlayplugin mac_fileproviderplugin nextcloud) # for the ownCloud.icns to be generated
    else()
        add_dependencies(mac_overlayplugin nextcloud) # for the ownCloud.icns to be generated
    endif()

    if (BUILD_OWNCLOUD_OSX_BUNDLE)
        install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Release/FinderSyncExt.appex
            DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns
            USE_SOURCE_PERMISSIONS)

        if (BUILD_FILE_PROVIDER_MODULE)
            install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Release/FileProviderExt.appex
                DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns
                USE_SOURCE_PERMISSIONS)
        endif()
    endif()
endif()

