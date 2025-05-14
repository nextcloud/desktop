# SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

if (LINUX)
    add_library(nextcloudsync_vfs_dbus SHARED
        hydrationjob.h
        hydrationjob.cpp
        vfs_dbusapi.h
        vfs_dbusapi.cpp
    )

    target_link_libraries(nextcloudsync_vfs_dbus PRIVATE
        Nextcloud::sync
    )

    set_target_properties(nextcloudsync_vfs_dbus
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
            ${APPLICATION_EXECUTABLE}sync_vfs_dbus
        RUNTIME_OUTPUT_NAME
            ${APPLICATION_EXECUTABLE}sync_vfs_dbus
    )

    target_include_directories(nextcloudsync_vfs_dbus BEFORE PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_BINARY_DIR})

    set(vfs_installdir "${PLUGINDIR}")

    generate_export_header(nextcloudsync_vfs_dbus
        BASE_NAME nextcloudsync_vfs_dbus
        EXPORT_MACRO_NAME NEXTCLOUD_CFAPI_EXPORT
        EXPORT_FILE_NAME cfapiexport.h
    )

    install(TARGETS nextcloudsync_vfs_dbus
        LIBRARY DESTINATION "${vfs_installdir}"
        RUNTIME DESTINATION "${vfs_installdir}"
    )
endif()
