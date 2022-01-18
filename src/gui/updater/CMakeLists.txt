if(APPLE AND NOT BUILD_LIBRARIES_ONLY)
    if (SPARKLE_FOUND)
        list(APPEND OWNCLOUD_BUNDLED_RESOURCES
            "${PROJECT_SOURCE_DIR}/admin/osx/deny_autoupdate_com.owncloud.desktopclient.plist"
            "${PROJECT_SOURCE_DIR}/admin/osx/sparkle/dsa_pub.pem")
        set(OWNCLOUD_BUNDLED_RESOURCES ${OWNCLOUD_BUNDLED_RESOURCES} PARENT_SCOPE)
    endif()
endif()

target_sources(owncloudCore PRIVATE
    ocupdater.cpp
    ocupdater.h
    updateinfo.cpp
    updateinfo.h
    updater.cpp
    updater.h
)

if(SPARKLE_FOUND)
    # Define this, we need to check in updater.cpp
    target_compile_definitions(owncloudCore PUBLIC HAVE_SPARKLE)
    target_sources(owncloudCore PRIVATE
        sparkleupdater_mac.mm
        sparkleupdater.h
    )
    target_link_libraries(owncloudCore PRIVATE ${SPARKLE_LIBRARY})
endif()
