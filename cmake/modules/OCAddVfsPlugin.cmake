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


    set_target_properties(vfs_${__PLUGIN_NAME} PROPERTIES OUTPUT_NAME "ownCloud_vfs_${__PLUGIN_NAME}")

    target_link_libraries(vfs_${__PLUGIN_NAME}
        libsync
        ${__PLUGIN_LIBS}
    )
    if(APPLE)
        set_target_properties(vfs_${__PLUGIN_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:owncloud>/../PlugIns/")
        # make the plugins available to the tests
        add_custom_command(TARGET vfs_${__PLUGIN_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND}
            ARGS -E create_symlink "$<TARGET_FILE:vfs_${__PLUGIN_NAME}>" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_FILE_NAME:vfs_${__PLUGIN_NAME}>" MAIN_DEPENDENCY "vfs_${__PLUGIN_NAME}")

    else()
        install(TARGETS vfs_${__PLUGIN_NAME} DESTINATION "${KDE_INSTALL_PLUGINDIR}")
    endif()

    if (TARGET owncloud)
        add_dependencies(owncloud vfs_${__PLUGIN_NAME})
    endif()
    if (TARGET cmd)
        add_dependencies(cmd vfs_${__PLUGIN_NAME})
    endif()
endfunction()
