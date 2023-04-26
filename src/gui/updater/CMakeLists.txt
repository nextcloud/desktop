if(APPLE)
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

if(WITH_APPIMAGEUPDATER)
    # needed for set_source_files_properties(... TARGET_DIRECTORY ...)
    cmake_minimum_required(VERSION 3.18)

    message(STATUS "Including built-in libappimageupdate based updater")

    set(appimageupdater_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdateavailabledialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdateavailabledialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdateavailabledialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdater.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdater.h
    )

    target_sources(owncloudCore PRIVATE ${appimageupdater_sources})

    # as libappimageupdate uses exceptions, we define a shim that catches all incoming exceptions
    # and handles them in a suitable way
    # we have to enable exceptions for this one compilation unit
    set_source_files_properties(${appimageupdater_sources}
        TARGET_DIRECTORY owncloudCore
        PROPERTIES COMPILE_OPTIONS "-fexceptions"
    )

    target_compile_definitions(owncloudCore PRIVATE WITH_APPIMAGEUPDATER)
    target_link_libraries(owncloudCore PRIVATE libappimageupdate-qt)

    find_package(Threads REQUIRED)
    target_link_libraries(owncloudCore PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()
