# SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(nextcloudsync_vfs_suffix SHARED
    vfs_suffix.h
    vfs_suffix.cpp
)

target_link_libraries(nextcloudsync_vfs_suffix PRIVATE Nextcloud::sync)

set_target_properties(nextcloudsync_vfs_suffix
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
      ${APPLICATION_EXECUTABLE}sync_vfs_suffix
    RUNTIME_OUTPUT_NAME
      ${APPLICATION_EXECUTABLE}sync_vfs_suffix
)

if(APPLE)
    # for being loadable when client run from build dir
    set(vfs_buildoutputdir "${BIN_OUTPUT_DIRECTORY}/${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns/")
    set_target_properties(nextcloudsync_vfs_suffix
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
        RUNTIME_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
        # macdeployqt (POST_BUILD) removes the build-tree RPATH (work/build/bin) that CMake
        # adds because this target depends on nextcloudsync. Without this flag CMake then tries
        # to delete that RPATH at cmake --install time via install_name_tool and fails because
        # macdeployqt has already eliminated it.
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    # For being loadable when client run from install dir (after make macdeployqt)
    set(vfs_installdir "${LIB_INSTALL_DIR}/../PlugIns")
else()
    set(vfs_installdir "${PLUGINDIR}")
endif()

install(TARGETS nextcloudsync_vfs_suffix
  LIBRARY DESTINATION "${vfs_installdir}"
  RUNTIME DESTINATION "${vfs_installdir}"
)

if (WIN32)
    install(FILES $<TARGET_PDB_FILE:nextcloudsync_vfs_suffix> DESTINATION bin OPTIONAL)
endif()
