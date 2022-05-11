# Globbing for plugins has a problem with in-source builds
# that create directories for the build.


include(OCAddVfsPlugin)

foreach(vfsPlugin ${VIRTUAL_FILE_SYSTEM_PLUGINS})
    set(vfsPluginPath ${vfsPlugin})
    set(vfsPluginPathOut ${vfsPlugin})
    get_filename_component(vfsPluginName ${vfsPlugin} NAME)
    if (NOT IS_ABSOLUTE ${vfsPlugin})
        set(vfsPluginPath "${CMAKE_CURRENT_LIST_DIR}/${vfsPlugin}")
    else()
        set(vfsPluginPathOut ${PROJECT_BINARY_DIR}/plugins/${vfsPluginName})
    endif()
    if(NOT IS_DIRECTORY ${vfsPluginPath})
        continue()
    endif()

    add_subdirectory(${vfsPluginPath} ${vfsPluginPathOut})

    if(BUILD_TESTING AND IS_DIRECTORY "${vfsPluginPath}/test")
        add_subdirectory("${vfsPluginPath}/test" "${vfsPluginName}_test")
        message(STATUS "Added vfsPlugin with tests: ${vfsPluginName}")
    else()
        message(STATUS "Added vfsPlugin without tests: ${vfsPluginName}")
    endif()
endforeach()
