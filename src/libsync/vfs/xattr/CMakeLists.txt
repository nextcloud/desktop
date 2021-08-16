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

    target_include_directories("${synclib_NAME}_vfs_xattr" BEFORE PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

    generate_export_header( "${synclib_NAME}_vfs_xattr"
        BASE_NAME "${synclib_NAME}_vfs_xattr"
        EXPORT_MACRO_NAME NEXTCLOUD_XATTR_EXPORT
        EXPORT_FILE_NAME xattrexport.h
    )

    install(TARGETS "${synclib_NAME}_vfs_xattr" DESTINATION "${KDE_INSTALL_PLUGINDIR}")
endif()
