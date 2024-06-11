find_package(nlohmann_json 3 QUIET)

option(WITH_WINDOWS_SHELL_EXTENSIONS "Whether to build the Windows shell extensions" ${nlohmann_json_FOUND})
add_feature_info("Windows shell extensions" WITH_WINDOWS_SHELL_EXTENSIONS "")

if (WITH_WINDOWS_SHELL_EXTENSIONS)
    find_package(nlohmann_json 3 REQUIRED)
    # Use static runtime for all subdirectories
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_subdirectory(OCContextMenu)
    add_subdirectory(OCOverlays)
    add_subdirectory(OCUtil)
endif()
