project(csync)

# global needed variables
set(APPLICATION_NAME ${PROJECT_NAME})
set(APPLICATION_VERSION "0.42")

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/cmake/Modules
)

# add definitions
include(DefineCMakeDefaults)
include(DefineCompilerFlags)
include(DefineInstallationPaths)
include(DefineOptions.cmake)

# disallow in-source build
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out of source build. Please create a separate build directory and run 'cmake /path/to/${PROJECT_NAME} [options]' there.")

# add macros
include(MacroAddPlugin)

if (WITH_LOG4C)
  find_package(LOG4C REQUIRED)
endif (WITH_LOG4C)
find_package(Check)

include(ConfigureChecks.cmake)
configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_subdirectory(iniparser/src)
add_subdirectory(src)
add_subdirectory(client)
add_subdirectory(config)
add_subdirectory(doc)

if (CHECK_FOUND AND UNIT_TESTING)
  include(MacroAddCheckTest)
  add_subdirectory(tests)
endif (CHECK_FOUND AND UNIT_TESTING)

