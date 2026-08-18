# SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2014 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
if(APPLE)
    if(MACOS_USE_ICON_COMPOSER)
        set(OC_OEM_SHARE_ICNS "${CMAKE_BINARY_DIR}/src/gui/AppIconCompiled/AppIcon.icns")
    else()
        set(OC_OEM_SHARE_ICNS "${CMAKE_BINARY_DIR}/src/gui/${APPLICATION_ICON_NAME}.icns")
    endif()

    if (NEXTCLOUD_DEV)
        set(XCODE_TARGET_CONFIGURATION "Debug")
    else()
        set(XCODE_TARGET_CONFIGURATION "Release")
    endif()

    # The bundle identifier and application group need to have compatible values with the client
    # to be able to open a Mach port across the extension's sandbox boundary.
    # Pass the info through the xcodebuild command line and make sure that the project uses
    # those user-defined settings to build the plist.
    #
    # DEVELOPMENT_TEAM has to be passed explicitly: the extension derives its
    # NCApplicationGroupIdentifier from it, and without it the Xcode project falls back to the
    # hardcoded default in Build.xcconfig. A branded build signed by a different team would
    # then point the extension at an app group it does not own, while the client — which takes
    # the team from CMake — points at the right one, and the two could never reach each other.
    add_custom_target( mac_overlayplugin ALL
        xcodebuild
        ARCHS=${CMAKE_OSX_ARCHITECTURES}
        ONLY_ACTIVE_ARCH=NO
        -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj
        -target FinderSyncExt
        -configuration ${XCODE_TARGET_CONFIGURATION}
        "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
        "MARKETING_VERSION=${MIRALL_VERSION}"
        "CURRENT_PROJECT_VERSION=${MIRALL_VERSION_BUILD}"
        "OC_OEM_SHARE_ICNS=${OC_OEM_SHARE_ICNS}"
        "OC_APPLICATION_NAME=${APPLICATION_NAME}"
        "OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}"
        "DEVELOPMENT_TEAM=${DEVELOPMENT_TEAM}"
        COMMENT building Mac Overlay icons
        VERBATIM)

    # The login item that vends the FinderSync XPC endpoint.
    #
    # Named once: the Xcode target name is also the built wrapper name and, via
    # PRODUCT_NAME, the last component of the bundle identifier assembled below.
    set(OSX_FINDERSYNC_BROKER_TARGET "FinderSyncBroker")

    add_custom_target( mac_findersyncbroker ALL
        xcodebuild
        ARCHS=${CMAKE_OSX_ARCHITECTURES}
        ONLY_ACTIVE_ARCH=NO
        -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj
        -target ${OSX_FINDERSYNC_BROKER_TARGET}
        -configuration ${XCODE_TARGET_CONFIGURATION}
        "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
        "MARKETING_VERSION=${MIRALL_VERSION}"
        "CURRENT_PROJECT_VERSION=${MIRALL_VERSION_BUILD}"
        "OC_APPLICATION_NAME=${APPLICATION_NAME}"
        "OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}"
        "DEVELOPMENT_TEAM=${DEVELOPMENT_TEAM}"
        COMMENT building macOS FinderSync broker login item
        VERBATIM)

    if (BUILD_FILE_PROVIDER_MODULE)
        add_custom_target( mac_fileproviderplugin ALL
            xcodebuild
            ARCHS=${CMAKE_OSX_ARCHITECTURES}
            ONLY_ACTIVE_ARCH=NO
            -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj
            -target FileProviderExt 
            -configuration ${XCODE_TARGET_CONFIGURATION}
            "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
            "MARKETING_VERSION=${MIRALL_VERSION}"
            "CURRENT_PROJECT_VERSION=${MIRALL_VERSION_BUILD}"
            "OC_APPLICATION_EXECUTABLE_NAME=${APPLICATION_EXECUTABLE}"
            "OC_APPLICATION_VENDOR=${APPLICATION_VENDOR}"
            "OC_APPLICATION_NAME=${APPLICATION_NAME}"
            "OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}"
            COMMENT building macOS File Provider extension
            VERBATIM)

        add_custom_target( mac_fileprovideruiplugin ALL
            xcodebuild
            ARCHS=${CMAKE_OSX_ARCHITECTURES}
            ONLY_ACTIVE_ARCH=NO
            -project ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj
            -target FileProviderUIExt
            -configuration ${XCODE_TARGET_CONFIGURATION}
            "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
            "MARKETING_VERSION=${MIRALL_VERSION}"
            "CURRENT_PROJECT_VERSION=${MIRALL_VERSION_BUILD}"
            "OC_APPLICATION_EXECUTABLE_NAME=${APPLICATION_EXECUTABLE}"
            "OC_APPLICATION_VENDOR=${APPLICATION_VENDOR}"
            "OC_APPLICATION_NAME=${APPLICATION_NAME}"
            "OC_APPLICATION_REV_DOMAIN=${APPLICATION_REV_DOMAIN}"
            DEPENDS mac_fileproviderplugin
            COMMENT building macOS File Provider UI extension
            VERBATIM)

        add_dependencies(mac_overlayplugin mac_fileproviderplugin mac_fileprovideruiplugin nextcloud) # for the ownCloud.icns to be generated
    else()
        add_dependencies(mac_overlayplugin nextcloud) # for the ownCloud.icns to be generated
    endif()

    # Serialize against the other xcodebuild targets: they all drive the same .xcodeproj with
    # the same SYMROOT, and concurrent invocations race on the shared build database.
    add_dependencies(mac_findersyncbroker mac_overlayplugin)

    if (BUILD_OWNCLOUD_OSX_BUNDLE)
        # Set debug entitlements conditionally based on build type
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(DEBUG_ENTITLEMENTS "\t<key>com.apple.security.get-task-allow</key>\n\t<true/>")
        else()
            set(DEBUG_ENTITLEMENTS "")
        endif()
        
        set(OSX_PLUGINS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/${XCODE_TARGET_CONFIGURATION})
        set(OSX_PLUGINS_INSTALL_DIR ${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns)

        configure_file(FinderSyncExt.entitlements.cmake ${CMAKE_CURRENT_BINARY_DIR}/FinderSyncExt.entitlements)

        install(DIRECTORY ${OSX_PLUGINS_BINARY_DIR}/FinderSyncExt.appex
            DESTINATION ${OSX_PLUGINS_INSTALL_DIR}
            USE_SOURCE_PERMISSIONS)

        # HACK: Launch Services expects there to be a Library folder within the FinderSyncExt.appex.
        install(DIRECTORY DESTINATION ${OSX_PLUGINS_INSTALL_DIR}/FinderSyncExt.appex/Contents/Library)

        # A Service Management login item must live in Contents/Library/LoginItems and its
        # wrapper filename must equal its bundle identifier, which is also the Mach service
        # name it vends. Xcode builds it as plain FinderSyncBroker.app so that no build setting
        # has to contain dots (they would break PRODUCT_MODULE_NAME for Swift), so the rename
        # happens here, where the branding variables live. The trailing slash on the source
        # installs the bundle's *contents* into the renamed wrapper.
        #
        # Mirrors PRODUCT_BUNDLE_IDENTIFIER of the FinderSyncBroker Xcode target, which is
        # $(DEVELOPMENT_TEAM).$(OC_APPLICATION_REV_DOMAIN).$(PRODUCT_NAME:rfc1034identifier).
        # Unlike its sibling extensions the identifier carries the team prefix, because launchd
        # synthesizes the login item's Mach service name from it and that name has to fall inside
        # the app group prefix for the sandboxed client and extension to look it up without a
        # temporary-exception entitlement. testfindersyncbrokeridentity asserts the agreement.
        set(OSX_FINDERSYNC_BROKER_BUNDLE_ID
            "${DEVELOPMENT_TEAM}.${APPLICATION_REV_DOMAIN}.${OSX_FINDERSYNC_BROKER_TARGET}")

        configure_file(FinderSyncBroker.entitlements.cmake ${CMAKE_CURRENT_BINARY_DIR}/FinderSyncBroker.entitlements)

        install(DIRECTORY ${OSX_PLUGINS_BINARY_DIR}/${OSX_FINDERSYNC_BROKER_TARGET}.app/
            DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Library/LoginItems/${OSX_FINDERSYNC_BROKER_BUNDLE_ID}.app
            USE_SOURCE_PERMISSIONS)

        if (BUILD_FILE_PROVIDER_MODULE)
            configure_file(FileProviderExt.entitlements.cmake ${CMAKE_CURRENT_BINARY_DIR}/FileProviderExt.entitlements)
            configure_file(FileProviderUIExt.entitlements.cmake ${CMAKE_CURRENT_BINARY_DIR}/FileProviderUIExt.entitlements)

            install(DIRECTORY ${OSX_PLUGINS_BINARY_DIR}/FileProviderExt.appex
                DESTINATION ${OSX_PLUGINS_INSTALL_DIR}
                USE_SOURCE_PERMISSIONS)

            install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${XCODE_TARGET_CONFIGURATION}/PackageFrameworks/RealmSwift.framework
                DESTINATION ${OSX_PLUGINS_INSTALL_DIR}/FileProviderExt.appex/Contents/Frameworks
                USE_SOURCE_PERMISSIONS)

            install(DIRECTORY ${OSX_PLUGINS_BINARY_DIR}/FileProviderUIExt.appex
                DESTINATION ${OSX_PLUGINS_INSTALL_DIR}
                USE_SOURCE_PERMISSIONS)

            install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${XCODE_TARGET_CONFIGURATION}/PackageFrameworks/RealmSwift.framework
                DESTINATION ${OSX_PLUGINS_INSTALL_DIR}/FileProviderUIExt.appex/Contents/Frameworks
                USE_SOURCE_PERMISSIONS)

            # File provider badge icon asset for items marked as "Always keep downloaded"
            install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/KeepDownloaded.icns
                DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources)
        endif()
    endif()
endif()

