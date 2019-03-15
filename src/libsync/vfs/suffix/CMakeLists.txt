add_library("${synclib_NAME}_vfs_suffix" SHARED
    vfs_suffix.cpp
)

target_link_libraries("${synclib_NAME}_vfs_suffix"
    "${synclib_NAME}"
)

set_target_properties("${synclib_NAME}_vfs_suffix" PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
    PREFIX ""
    AUTOMOC TRUE
)

if(APPLE)
    # for being loadable when client run from build dir
    set(vfs_buildoutputdir "${BIN_OUTPUT_DIRECTORY}/${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns/")
    set_target_properties("${synclib_NAME}_vfs_suffix"
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
        RUNTIME_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
    )
    # For being lodable when client run from install dir (after make macdeployqt)
    set(vfs_installdir "${LIB_INSTALL_DIR}/../PlugIns")
else()
    set(vfs_installdir "${PLUGINDIR}")
endif()

INSTALL(TARGETS "${synclib_NAME}_vfs_suffix"
  LIBRARY DESTINATION "${vfs_installdir}"
  RUNTIME DESTINATION "${vfs_installdir}"
)

