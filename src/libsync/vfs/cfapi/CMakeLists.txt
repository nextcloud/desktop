# SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
if (WIN32)
    add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_WIN10)

    add_library(nextcloudsync_vfs_cfapi SHARED
        cfapiwrapper.h
        cfapiwrapper.cpp
        hydrationjob.h
        hydrationjob.cpp
        vfs_cfapi.h
        vfs_cfapi.cpp
    )
    
	add_subdirectory(shellext)

    target_link_libraries(nextcloudsync_vfs_cfapi PRIVATE
        Nextcloud::sync
        cldapi
    )

    set_target_properties(nextcloudsync_vfs_cfapi
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
          ${APPLICATION_EXECUTABLE}sync_vfs_cfapi
        RUNTIME_OUTPUT_NAME
          ${APPLICATION_EXECUTABLE}sync_vfs_cfapi
    )

    target_include_directories("nextcloudsync_vfs_cfapi" BEFORE PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_BINARY_DIR})

    set(vfs_installdir "${PLUGINDIR}")

    generate_export_header(nextcloudsync_vfs_cfapi
        BASE_NAME nextcloudsync_vfs_cfapi
        EXPORT_MACRO_NAME NEXTCLOUD_CFAPI_EXPORT
        EXPORT_FILE_NAME cfapiexport.h
    )

    install(TARGETS nextcloudsync_vfs_cfapi
      LIBRARY DESTINATION "${vfs_installdir}"
      RUNTIME DESTINATION "${vfs_installdir}"
    )

    install(FILES $<TARGET_PDB_FILE:nextcloudsync_vfs_cfapi> DESTINATION bin OPTIONAL)
endif()
