if(APPLE)
    if (SPARKLE_FOUND)
        list(APPEND OWNCLOUD_BUNDLED_RESOURCES
            "${PROJECT_SOURCE_DIR}/admin/osx/deny_autoupdate_com.owncloud.desktopclient.plist"
            "${PROJECT_SOURCE_DIR}/admin/osx/sparkle/dsa_pub.pem")
        set(OWNCLOUD_BUNDLED_RESOURCES ${OWNCLOUD_BUNDLED_RESOURCES} PARENT_SCOPE)
    endif()
endif()

target_sources(owncloudGui PRIVATE
    ocupdater.cpp
    ocupdater.h
    newversionavailabledialog.cpp
    newversionavailabledialog.h
    newversionavailabledialog.ui
    updateinfo.cpp
    updateinfo.h
    updater.cpp
    updater.h
    updatedownloadeddialog.cpp
    updatedownloadeddialog.h
    updatedownloadeddialog.ui
)

if(SPARKLE_FOUND)
    # Define this, we need to check in updater.cpp
    target_compile_definitions(owncloudGui PUBLIC HAVE_SPARKLE)
    target_sources(owncloudGui PRIVATE
        sparkleupdater_mac.mm
        sparkleupdater.h
    )
    target_link_libraries(owncloudGui PRIVATE ${SPARKLE_LIBRARY})
endif()

if(WITH_APPIMAGEUPDATER)
    message(STATUS "Including built-in libappimageupdate based updater")

    set(appimageupdater_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdateavailabledialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdateavailabledialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdateavailabledialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdater.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/appimageupdater.h
    )

    target_sources(owncloudGui PRIVATE ${appimageupdater_sources})

    # as libappimageupdate uses exceptions, we define a shim that catches all incoming exceptions
    # and handles them in a suitable way
    # we have to enable exceptions for this one compilation unit
    set_source_files_properties(${appimageupdater_sources}
        TARGET_DIRECTORY owncloudGui
        PROPERTIES COMPILE_OPTIONS "-fexceptions"
    )

    target_compile_definitions(owncloudGui PRIVATE WITH_APPIMAGEUPDATER)
    target_link_libraries(owncloudGui PRIVATE libappimageupdate)

    find_package(Threads REQUIRED)
    target_link_libraries(owncloudGui PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()
