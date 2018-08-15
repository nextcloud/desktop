### TODO: Find plugins dynamically
list(APPEND vfsPlugins "suffix")

foreach(vfsPlugin ${vfsPlugins})
    message(STATUS "Add vfsPlugin in dir: ${vfsPlugin}")
    add_subdirectory("${vfsPlugin}")

    if(UNIT_TESTING AND IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/${vfsPlugin}/test")
        message(STATUS "Add vfsPlugin tests in dir: ${vfsPlugin}")
        add_subdirectory("${vfsPlugin}/test" "${vfsPlugin}_test")
    endif()
endforeach()
