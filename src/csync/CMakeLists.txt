project(libcsync)
set(CMAKE_AUTOMOC TRUE)
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

find_package(SQLite3 3.8.0 REQUIRED)

include(ConfigureChecks.cmake)
include(../common/common.cmake)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if (MEM_NULL_TESTS)
  add_definitions(-DCSYNC_MEM_NULL_TESTS)
endif (MEM_NULL_TESTS)

add_subdirectory(std)

# Statically include sqlite

set(CSYNC_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  CACHE INTERNAL "csync public include directories"
)

set(CSYNC_PRIVATE_INCLUDE_DIRS
  ${SQLITE3_INCLUDE_DIRS}
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
)

set(CSYNC_LIBRARY
  ocsync
  CACHE INTERNAL "ocsync library"
)

set(CSYNC_LINK_LIBRARIES
  ${CSTDLIB_LIBRARY}
  ${CSYNC_REQUIRED_LIBRARIES}
  ${SQLITE3_LIBRARIES}
)

# Specific option for builds tied to servers that do not support renaming extensions
set(NO_RENAME_EXTENSION 0 CACHE BOOL "Do not issue rename if the extension changes")
if(NO_RENAME_EXTENSION)
    add_definitions(-DNO_RENAME_EXTENSION)
endif()

set(csync_SRCS
  csync.cpp
  csync_exclude.cpp
  csync_log.cpp
  csync_statedb.cpp
  csync_time.c
  csync_util.cpp
  csync_misc.cpp

  csync_update.cpp
  csync_reconcile.cpp

  csync_rename.cpp

  vio/csync_vio.cpp
)

if (WIN32)
    list(APPEND csync_SRCS
        vio/csync_vio_local_win.cpp
    )
else()
    list(APPEND csync_SRCS
        vio/csync_vio_local_unix.cpp
    )
endif()


configure_file(csync_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/csync_version.h)

set(csync_HDRS
  ${CMAKE_CURRENT_BINARY_DIR}/csync_version.h
  csync.h
  vio/csync_vio.h
  vio/csync_vio_method.h
  vio/csync_vio_module.h
)

# Statically include sqlite
if (USE_OUR_OWN_SQLITE3)
    list(APPEND csync_SRCS ${SQLITE3_SOURCE})
    if (WIN32)
        # We want to export sqlite symbols from the ocsync DLL without
        # having to patch both sqlite3.h and the amalgation sqlite3.c,
        # so do the import/export magic manually through the build system.
        remove_definitions(-DSQLITE_API=__declspec\(dllimport\))
        add_definitions(-DSQLITE_API=__declspec\(dllexport\))
    endif()
endif()

include_directories(
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${CSYNC_PRIVATE_INCLUDE_DIRS}
)

add_library(${CSYNC_LIBRARY} SHARED ${common_SOURCES} ${csync_SRCS})
#add_library(${CSYNC_LIBRARY}_static STATIC ${csync_SRCS})

generate_export_header( ${CSYNC_LIBRARY}
  EXPORT_MACRO_NAME OCSYNC_EXPORT
  EXPORT_FILE_NAME ocsynclib.h
)

target_link_libraries(${CSYNC_LIBRARY} ${CSYNC_LINK_LIBRARIES})
#target_link_libraries(${CSYNC_LIBRARY}_static ${CSYNC_LINK_LIBRARIES})

if(ZLIB_FOUND)
  target_link_libraries(${CSYNC_LIBRARY} ${ZLIB_LIBRARIES})
  include_directories(${ZLIB_INCLUDE_DIRS})
endif(ZLIB_FOUND)

find_package(Qt5Core REQUIRED)
qt5_use_modules(${CSYNC_LIBRARY} Core Concurrent)

# For src/common/utility_mac.cpp
if (APPLE)
    find_library(FOUNDATION_LIBRARY NAMES Foundation)
    find_library(CORESERVICES_LIBRARY NAMES CoreServices)
    target_link_libraries(${CSYNC_LIBRARY} ${FOUNDATION_LIBRARY} ${CORESERVICES_LIBRARY})
endif()

set_target_properties(
  ${CSYNC_LIBRARY}
    PROPERTIES
      VERSION
        ${LIBRARY_VERSION}
      SOVERSION
        ${LIBRARY_SOVERSION}
      RUNTIME_OUTPUT_DIRECTORY
        ${BIN_OUTPUT_DIRECTORY}
)
if(BUILD_OWNCLOUD_OSX_BUNDLE)
  INSTALL(
    TARGETS
      ${CSYNC_LIBRARY}
    LIBRARY DESTINATION
	${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION
	${LIB_INSTALL_DIR}
    RUNTIME DESTINATION
	${BIN_INSTALL_DIR}
    )
else()
  INSTALL(
  TARGETS
    ${CSYNC_LIBRARY}
  LIBRARY DESTINATION
    ${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE}
  ARCHIVE DESTINATION
    ${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE}
  RUNTIME DESTINATION
    ${BIN_INSTALL_DIR}/${APPLICATION_EXECUTABLE}
  )
endif()

configure_file(config_csync.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config_csync.h)
