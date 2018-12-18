# Globbing for plugins has a problem with in-source builds
# that create directories for the build.
#file(GLOB vfsPlugins RELATIVE ${CMAKE_CURRENT_LIST_DIR} "*")

SET(vfsPlugins "suffix;win")

foreach(vfsPlugin ${vfsPlugins})
    if(NOT IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/${vfsPlugin}")
        continue()
    endif()

    add_subdirectory("${vfsPlugin}")

    if(UNIT_TESTING AND IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/${vfsPlugin}/test")
        add_subdirectory("${vfsPlugin}/test" "${vfsPlugin}_test")
        message(STATUS "Added vfsPlugin with tests: ${vfsPlugin}")
    else()
        message(STATUS "Added vfsPlugin without tests: ${vfsPlugin}")
    endif()
endforeach()
