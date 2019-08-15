# Globbing for plugins has a problem with in-source builds
# that create directories for the build.
#file(GLOB VIRTUAL_FILE_SYSTEM_PLUGINS RELATIVE ${CMAKE_CURRENT_LIST_DIR} "*")

list(APPEND VIRTUAL_FILE_SYSTEM_PLUGINS "suffix" "win")

foreach(vfsPlugin ${VIRTUAL_FILE_SYSTEM_PLUGINS})
    set(vfsPluginPath ${vfsPlugin})
    get_filename_component(vfsPluginName ${vfsPlugin} NAME)
    if (NOT IS_ABSOLUTE ${vfsPlugin})
        set(vfsPluginPath "${CMAKE_CURRENT_LIST_DIR}/${vfsPlugin}")
    endif()
    if(NOT IS_DIRECTORY ${vfsPluginPath})
        continue()
    endif()

    add_subdirectory(${vfsPluginPath} ${vfsPluginName})

    if(UNIT_TESTING AND IS_DIRECTORY "${vfsPluginPath}/test")
        add_subdirectory("${vfsPluginPath}/test" "${vfsPluginName}_test")
        message(STATUS "Added vfsPlugin with tests: ${vfsPluginName}")
    else()
        message(STATUS "Added vfsPlugin without tests: ${vfsPluginName}")
    endif()
endforeach()
