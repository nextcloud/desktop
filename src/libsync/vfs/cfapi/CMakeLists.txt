if (WIN32)
    add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_WIN10)

    add_library("${synclib_NAME}_vfs_cfapi" SHARED
        cfapiwrapper.cpp
        hydrationjob.cpp
        vfs_cfapi.cpp
    )

    target_link_libraries("${synclib_NAME}_vfs_cfapi"
        "${synclib_NAME}"
        cldapi
    )

    target_include_directories("${synclib_NAME}_vfs_cfapi" BEFORE PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_BINARY_DIR})

    set(vfs_installdir "${PLUGINDIR}")

    generate_export_header("${synclib_NAME}_vfs_cfapi"
        BASE_NAME "${synclib_NAME}_vfs_cfapi"
        EXPORT_MACRO_NAME NEXTCLOUD_CFAPI_EXPORT
        EXPORT_FILE_NAME cfapiexport.h
    )

    install(TARGETS "${synclib_NAME}_vfs_cfapi" DESTINATION "${KDE_INSTALL_PLUGINDIR}")
endif()
