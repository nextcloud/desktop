if(APPLE AND NOT BUILD_LIBRARIES_ONLY)
    if (SPARKLE_FOUND)
        list(APPEND OWNCLOUD_BUNDLED_RESOURCES ${PROJECT_SOURCE_DIR}/admin/osx/deny_autoupdate_com.owncloud.desktopclient.plist
            ${PROJECT_SOURCE_DIR}/admin/osx/sparkle/dsa_pub.pem)
        set(OWNCLOUD_BUNDLED_RESOURCES ${OWNCLOUD_BUNDLED_RESOURCES} PARENT_SCOPE)
    endif()
endif()
