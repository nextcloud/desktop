if(BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    if (SPARKLE_FOUND)
        install(FILES ${CMAKE_SOURCE_DIR}/admin/osx/deny_autoupdate_com.owncloud.desktopclient.plist
                DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources)
        install(FILES ${CMAKE_SOURCE_DIR}/admin/osx/sparkle/dsa_pub.pem
                DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources)
    endif()
endif()

