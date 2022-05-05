add_library(owncloud_vfs_suffix MODULE
    vfs_suffix.cpp
)

set_target_properties(owncloud_vfs_suffix PROPERTIES OUTPUT_NAME "${synclib_NAME}_vfs_suffix")

target_link_libraries(owncloud_vfs_suffix
    libsync
)
INSTALL(TARGETS owncloud_vfs_suffix DESTINATION "${KDE_INSTALL_PLUGINDIR}")

