target_sources(libsync PRIVATE
        jobs/drives.cpp
        space.cpp
        spacesmanager.cpp)

target_include_directories(libsync PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
