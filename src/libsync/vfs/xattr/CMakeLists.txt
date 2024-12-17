if (LINUX)
    set(vfs_xattr_SRCS
        vfs_xattr.h
        vfs_xattr.cpp
        xattrwrapper.h
        xattrwrapper_linux.cpp
    )

    add_library(nextcloudsync_vfs_xattr SHARED
        ${vfs_xattr_SRCS}
    )

    target_link_libraries(nextcloudsync_vfs_xattr PRIVATE Nextcloud::sync)

    set_target_properties(nextcloudsync_vfs_xattr
      PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY
          ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY
          ${BIN_OUTPUT_DIRECTORY}
        PREFIX
          ""
        AUTOMOC
          TRUE
        LIBRARY_OUTPUT_NAME
          ${APPLICATION_EXECUTABLE}sync_vfs_xattr
        RUNTIME_OUTPUT_NAME
          ${APPLICATION_EXECUTABLE}sync_vfs_xattr
    )

    target_include_directories(nextcloudsync_vfs_xattr BEFORE PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

    if(APPLE)
        # for being loadable when client run from build dir
        set(vfs_buildoutputdir "${BIN_OUTPUT_DIRECTORY}/${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns/")
        set_target_properties(nextcloudsync_vfs_xattr
            PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
            RUNTIME_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
        )
        # For being loadable when client run from install dir (after make macdeployqt)
        set(vfs_installdir "${LIB_INSTALL_DIR}/../PlugIns")
    else()
        set(vfs_installdir "${PLUGINDIR}")
    endif()

    generate_export_header(nextcloudsync_vfs_xattr
        BASE_NAME nextcloudsync_vfs_xattr
        EXPORT_MACRO_NAME NEXTCLOUD_XATTR_EXPORT
        EXPORT_FILE_NAME xattrexport.h
    )

    install(TARGETS nextcloudsync_vfs_xattr
      LIBRARY DESTINATION "${vfs_installdir}"
      RUNTIME DESTINATION "${vfs_installdir}"
    )

    if (WIN32)
        install(FILES $<TARGET_PDB_FILE:nextcloudsync_vfs_xattr> DESTINATION bin OPTIONAL)
    endif()
endif()
