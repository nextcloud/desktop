# TODO: OSX and LIB_ONLY seem to require this to go to binary dir only
set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(synclib_NAME ${APPLICATION_EXECUTABLE}sync)

add_subdirectory(libsync)
if (NOT BUILD_LIBRARIES_ONLY)
    add_subdirectory(gui)
    add_subdirectory(cmd)
endif(NOT BUILD_LIBRARIES_ONLY)

find_program(KRAZY2_EXECUTABLE krazy2)
if(KRAZY2_EXECUTABLE)
    # s/y k/y ALL k/ for building this target always
    add_custom_target( krazy krazy2 --check-sets c++,qt4,foss
                       ${PROJECT_SOURCE_DIR}/src/mirall/*.ui
                       ${PROJECT_SOURCE_DIR}/src/mirall/*.h
                       ${PROJECT_SOURCE_DIR}/src/mirall/*.cpp
)
endif()
