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

find_package(SQLite3 3.8.0 REQUIRED)

include(ConfigureChecks.cmake)
include(../common/common.cmake)

if (MEM_NULL_TESTS)
  add_definitions(-DCSYNC_MEM_NULL_TESTS)
endif (MEM_NULL_TESTS)

# Specific option for builds tied to servers that do not support renaming extensions
set(NO_RENAME_EXTENSION 0 CACHE BOOL "Do not issue rename if the extension changes")
if(NO_RENAME_EXTENSION)
    add_definitions(-DNO_RENAME_EXTENSION)
endif()

set(csync_SRCS
  csync.cpp
  csync_exclude.cpp
  csync_util.cpp
  csync_misc.cpp

  std/c_alloc.c
  std/c_string.c
  std/c_time.c
  std/c_utf8.cpp

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

if(NOT HAVE_ASPRINTF AND NOT HAVE___MINGW_ASPRINTF)
    list(APPEND csync_SRCS std/asprintf.c)
endif()

if (USE_OUR_OWN_SQLITE3)
    list(APPEND csync_SRCS ${SQLITE3_SOURCE})
endif()

configure_file(csync_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/csync_version.h)

add_library("${csync_NAME}" SHARED ${common_SOURCES} ${csync_SRCS})

target_include_directories(
  "${csync_NAME}"
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/std
)

find_package(SQLite3 3.8.0 REQUIRED)
if (USE_OUR_OWN_SQLITE3)
    # make sure that the path for the local sqlite3 is before the system one
    target_include_directories("${csync_NAME}" BEFORE PRIVATE ${SQLITE3_INCLUDE_DIR})
else()
    target_include_directories("${csync_NAME}" PRIVATE ${SQLITE3_INCLUDE_DIR})
endif()


generate_export_header("${csync_NAME}"
  EXPORT_MACRO_NAME OCSYNC_EXPORT
  EXPORT_FILE_NAME ocsynclib.h
)

target_link_libraries("${csync_NAME}"
  ${CSYNC_REQUIRED_LIBRARIES}
  ${SQLITE3_LIBRARIES}
  Qt5::Core Qt5::Concurrent
)

if(ZLIB_FOUND)
  target_link_libraries("${csync_NAME}" ZLIB::ZLIB)
endif(ZLIB_FOUND)


# For src/common/utility_mac.cpp
if (APPLE)
    find_library(FOUNDATION_LIBRARY NAMES Foundation)
    find_library(CORESERVICES_LIBRARY NAMES CoreServices)
    target_link_libraries("${csync_NAME}" ${FOUNDATION_LIBRARY} ${CORESERVICES_LIBRARY})
endif()

set_target_properties(
  "${csync_NAME}"
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
    "${csync_NAME}"
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
  "${csync_NAME}"
  LIBRARY DESTINATION
    ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION
    ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION
    ${CMAKE_INSTALL_BINDIR}
  )
endif()

configure_file(config_csync.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config_csync.h)
