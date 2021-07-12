if (LINUX)
    set(vfs_xattr_SRCS
        vfs_xattr.cpp
        xattrwrapper_linux.cpp
    )

    add_library("${synclib_NAME}_vfs_xattr" SHARED
        ${vfs_xattr_SRCS}
    )

    target_link_libraries("${synclib_NAME}_vfs_xattr"
        "${synclib_NAME}"
    )

    set_target_properties("${synclib_NAME}_vfs_xattr" PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
        PREFIX ""
        AUTOMOC TRUE
    )

    target_include_directories("${synclib_NAME}_vfs_xattr" BEFORE PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

    if(APPLE)
        # for being loadable when client run from build dir
        set(vfs_buildoutputdir "${BIN_OUTPUT_DIRECTORY}/${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns/")
        set_target_properties("${synclib_NAME}_vfs_xattr"
            PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
            RUNTIME_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
        )
        # For being loadable when client run from install dir (after make macdeployqt)
        set(vfs_installdir "${LIB_INSTALL_DIR}/../PlugIns")
    else()
        set(vfs_installdir "${PLUGINDIR}")
    endif()

    GENERATE_EXPORT_HEADER( "${synclib_NAME}_vfs_xattr"
        BASE_NAME "${synclib_NAME}_vfs_xattr"
        EXPORT_MACRO_NAME NEXTCLOUD_XATTR_EXPORT
        EXPORT_FILE_NAME xattrexport.h
    )

    INSTALL(TARGETS "${synclib_NAME}_vfs_xattr"
      LIBRARY DESTINATION "${vfs_installdir}"
      RUNTIME DESTINATION "${vfs_installdir}"
    )
endif()
