# TODO: OSX and LIB_ONLY seem to require this to go to binary dir only
if(NOT TOKEN_AUTH_ONLY)
endif()

set(synclib_NAME ${APPLICATION_EXECUTABLE}sync)

if(NOT TOKEN_AUTH_ONLY)
    find_package(Qt5Keychain REQUIRED)
endif()

add_subdirectory(csync)
add_subdirectory(libsync)
if (NOT BUILD_LIBRARIES_ONLY)
    add_subdirectory(gui)
    add_subdirectory(cmd)

    if (WITH_CRASHREPORTER)
        add_subdirectory(3rdparty/libcrashreporter-qt)
        add_subdirectory(crashreporter)
    endif()
endif(NOT BUILD_LIBRARIES_ONLY)

find_program(KRAZY2_EXECUTABLE krazy2)
if(KRAZY2_EXECUTABLE)
    # s/y k/y ALL k/ for building this target always
    add_custom_target( krazy krazy2 --check-sets c++,qt4,foss
                       ${PROJECT_SOURCE_DIR}/src/libsync/*.ui
                       ${PROJECT_SOURCE_DIR}/src/libsync/*.h
                       ${PROJECT_SOURCE_DIR}/src/libsync/*.cpp
                       ${PROJECT_SOURCE_DIR}/src/gui/*.ui
                       ${PROJECT_SOURCE_DIR}/src/gui/*.h
                       ${PROJECT_SOURCE_DIR}/src/gui/*.cpp
                       ${PROJECT_SOURCE_DIR}/src/cmd/*.h
                       ${PROJECT_SOURCE_DIR}/src/cmd/*.cpp
)
endif()
