# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2025 OpenCloud GmbH and OpenCloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

if (NOT WIN32)
    find_package(OpenVFS 0.1.0 QUIET)
    set_package_properties(OpenVFS PROPERTIES TYPE OPTIONAL
            DESCRIPTION "A Virtual Filesystem Layer for cloud storages for the free desktop"
            URL "https://github.com/opencloud-eu/openvfs/")

    if(TARGET OpenVFS::OpenVFS)
        get_target_property (OPEN_VFS_CONFIG OpenVFS::OpenVFS IMPORTED_CONFIGURATIONS)
        get_target_property (OPEN_VFS_PATH OpenVFS::OpenVFS IMPORTED_LOCATION_${OPEN_VFS_CONFIG})
        message(STATUS "Using OpenVFS ${OPEN_VFS_CONFIG} (${OPEN_VFS_PATH})")

        set(vfs_openvfs_SRCS
            vfs_openvfs.h
            vfs_openvfs.cpp
            xattr.h
            xattr.cpp
            hydrationjob.h
            hydrationjob.cpp
        )

        add_library(nextcloudsync_vfs_openvfs SHARED
            ${vfs_openvfs_SRCS}
        )

        target_link_libraries(nextcloudsync_vfs_openvfs PRIVATE Nextcloud::sync OpenVFS::LibOpenVFS)

        target_compile_definitions(nextcloudsync_vfs_openvfs PRIVATE OPENVFS_EXE=\"${OPEN_VFS_PATH}\")

        set_target_properties(nextcloudsync_vfs_openvfs
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
              ${APPLICATION_EXECUTABLE}sync_vfs_openvfs
            RUNTIME_OUTPUT_NAME
              ${APPLICATION_EXECUTABLE}sync_vfs_openvfs
        )

        target_include_directories(nextcloudsync_vfs_openvfs BEFORE PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

        if(APPLE)
            # for being loadable when client run from build dir
            set(vfs_buildoutputdir "${BIN_OUTPUT_DIRECTORY}/${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns/")
            set_target_properties(nextcloudsync_vfs_openvfs
                PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
                RUNTIME_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
            )
            # For being loadable when client run from install dir (after make macdeployqt)
            set(vfs_installdir "${LIB_INSTALL_DIR}/../PlugIns")
        else()
            set(vfs_installdir "${PLUGINDIR}")
        endif()

        generate_export_header(nextcloudsync_vfs_openvfs
            BASE_NAME nextcloudsync_vfs_openvfs
            EXPORT_MACRO_NAME NEXTCLOUD_OPENVFS_EXPORT
            EXPORT_FILE_NAME openvfsexport.h
        )

        install(TARGETS nextcloudsync_vfs_openvfs
          LIBRARY DESTINATION "${vfs_installdir}"
          RUNTIME DESTINATION "${vfs_installdir}"
        )

        if (WIN32)
            install(FILES $<TARGET_PDB_FILE:nextcloudsync_vfs_openvfs> DESTINATION bin OPTIONAL)
        endif()
    endif()
endif()
