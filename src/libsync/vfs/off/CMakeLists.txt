add_library(owncloud_vfs_off MODULE
    vfs_off.cpp
)

set_target_properties(owncloud_vfs_off PROPERTIES OUTPUT_NAME "${synclib_NAME}_vfs_off")

target_link_libraries(owncloud_vfs_off PRIVATE
    libsync
)
INSTALL(TARGETS owncloud_vfs_off DESTINATION "${KDE_INSTALL_PLUGINDIR}")

