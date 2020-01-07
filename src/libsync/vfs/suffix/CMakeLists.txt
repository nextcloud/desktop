add_library("${synclib_NAME}_vfs_suffix" MODULE
    vfs_suffix.cpp
)

target_link_libraries("${synclib_NAME}_vfs_suffix"
    "${synclib_NAME}"
)
INSTALL(TARGETS "${synclib_NAME}_vfs_suffix" DESTINATION "${KDE_INSTALL_PLUGINDIR}")

