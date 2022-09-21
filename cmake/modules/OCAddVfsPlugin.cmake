include(OCApplyCommonSettings)

function(add_vfs_plugin)
    set(options "")
    set(oneValueArgs NAME)
    set(multiValueArgs SRC LIBS)
    cmake_parse_arguments(__PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_library(vfs_${__PLUGIN_NAME} MODULE
        ${__PLUGIN_SRC}
    )
    apply_common_target_settings(vfs_${__PLUGIN_NAME})


    set_target_properties(vfs_${__PLUGIN_NAME} PROPERTIES OUTPUT_NAME "${synclib_NAME}_vfs_${__PLUGIN_NAME}")

    target_link_libraries(vfs_${__PLUGIN_NAME}
        libsync
        ${__PLUGIN_LIBS}
    )
    INSTALL(TARGETS vfs_${__PLUGIN_NAME} DESTINATION "${KDE_INSTALL_PLUGINDIR}")

    if (TARGET owncloud)
        add_dependencies(owncloud vfs_${__PLUGIN_NAME})
    endif()
    if (TARGET cmd)
        add_dependencies(cmd vfs_${__PLUGIN_NAME})
    endif()
endfunction()