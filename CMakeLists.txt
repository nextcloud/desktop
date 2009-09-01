project(csync C)

# Required cmake version
cmake_minimum_required(VERSION 2.4.6)

# global needed variables
set(APPLICATION_NAME ${PROJECT_NAME})

set(APPLICATION_VERSION "0.43.0")

set(APPLICATION_VERSION_MAJOR "0")
set(APPLICATION_VERSION_MINOR "43")
set(APPLICATION_VERSION_PATCH "0")

set(LIBRARY_VERSION "0.1.1")
set(LIBRARY_SOVERSION "0")

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/cmake/Modules
)

# add definitions
include(DefineCMakeDefaults)
include(DefineCompilerFlags)
include(DefineInstallationPaths)
include(DefineOptions.cmake)
include(CPackConfig.cmake)

# disallow in-source build
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out of source build. Please create a separate build directory and run 'cmake /path/to/${PROJECT_NAME} [options]' there.")

# add macros
include(MacroAddPlugin)
include(MacroCopyFile)

if (WITH_LOG4C)
  find_package(Log4C REQUIRED)
endif (WITH_LOG4C)

include(ConfigureChecks.cmake)
configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

macro_copy_file(${CMAKE_CURRENT_SOURCE_DIR}/CTestCustom.cmake ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake)

if (MEM_NULL_TESTS)
  add_definitions(-DCSYNC_MEM_NULL_TESTS)
endif (MEM_NULL_TESTS)

add_subdirectory(src)
add_subdirectory(modules)
add_subdirectory(client)
add_subdirectory(config)
add_subdirectory(doc)

find_package(Check)
if (CHECK_FOUND AND UNIT_TESTING)
  include(MacroAddCheckTest)
  add_subdirectory(tests)
endif (CHECK_FOUND AND UNIT_TESTING)

