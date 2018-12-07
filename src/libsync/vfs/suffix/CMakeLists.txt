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

INSTALL(TARGETS "${synclib_NAME}_vfs_suffix" LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/plugins")

