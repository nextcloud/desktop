

# global needed variables
set(APPLICATION_NAME "ocsync")

set(LIBRARY_VERSION ${MIRALL_VERSION})
set(LIBRARY_SOVERSION "0")

# add definitions
include(DefineCMakeDefaults)
include(DefinePlatformDefaults)
include(DefineCompilerFlags)
include(DefineOptions.cmake)

include(DefineInstallationPaths)

# add macros
include(MacroAddPlugin)
include(MacroCopyFile)

if (NOT WIN32)
    find_package(Iconv)
endif (NOT WIN32)

find_package(SQLite3 3.8.0 REQUIRED)

include(ConfigureChecks.cmake)


set(SOURCE_DIR ${CMAKE_SOURCE_DIR})

include_directories(${CMAKE_CURRENT_BINARY_DIR})

macro_copy_file(${CMAKE_CURRENT_SOURCE_DIR}/CTestCustom.cmake ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake)

if (MEM_NULL_TESTS)
  add_definitions(-DCSYNC_MEM_NULL_TESTS)
endif (MEM_NULL_TESTS)

add_subdirectory(src)

if (UNIT_TESTING)
    set(WITH_TESTING ON)

    find_package(CMocka)
    if (CMOCKA_FOUND)
        include(AddCMockaTest)
        add_subdirectory(tests)
    endif (CMOCKA_FOUND)
endif (UNIT_TESTING)

configure_file(config_csync.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config_csync.h)
configure_file(config_test.h.cmake  ${CMAKE_CURRENT_BINARY_DIR}/config_test.h)


