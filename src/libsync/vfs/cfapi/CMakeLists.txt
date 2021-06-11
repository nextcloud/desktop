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

    set_target_properties("${synclib_NAME}_vfs_cfapi" PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
        PREFIX ""
        AUTOMOC TRUE
    )

    target_include_directories("${synclib_NAME}_vfs_cfapi" BEFORE PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_BINARY_DIR})

    set(vfs_installdir "${PLUGINDIR}")

    GENERATE_EXPORT_HEADER( "${synclib_NAME}_vfs_cfapi"
        BASE_NAME "${synclib_NAME}_vfs_cfapi"
        EXPORT_MACRO_NAME NEXTCLOUD_CFAPI_EXPORT
        EXPORT_FILE_NAME cfapiexport.h
    )

    INSTALL(TARGETS "${synclib_NAME}_vfs_cfapi"
      LIBRARY DESTINATION "${vfs_installdir}"
      RUNTIME DESTINATION "${vfs_installdir}"
    )
endif()
