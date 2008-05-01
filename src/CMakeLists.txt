project(libcsync)

add_subdirectory(std)

find_package(Sqlite3 REQUIRED)
find_package(Iniparser REQUIRED)

set(CSYNC_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  CACHE INTERNAL "csync public include directories"
)

set(CSYNC_PRIVATE_INCLUDE_DIRS
  ${SQLITE3_INCLUDE_DIRS}
  ${INIPARSER_INCLUDE_DIRS}
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
)

set(CSYNC_LIBRARY
  csync
  CACHE INTERNAL "csync library"
)

set(CSYNC_LINK_LIBRARIES
  ${CSYNC_LIBRARY}
  ${CSTDLIB_LIBRARY}
  ${INIPARSER_LIBRARIES}
  ${LOG4C_LIBRARIES}
  ${SQLITE3_LIBRARIES}
  dl
)

set(csync_SRCS
  csync.c
  csync_config.c
  csync_exclude.c
  csync_journal.c
  csync_lock.c
  csync_util.c
  csync_update.c

  vio/csync_vio.c
  vio/csync_vio_handle.c
  vio/csync_vio_file_stat.c
  vio/csync_vio_local.c
)

set(csync_HDRS
  csync.h
)

include_directories(
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${CSYNC_PRIVATE_INCLUDE_DIRS}
)

add_library(${CSYNC_LIBRARY} SHARED ${csync_SRCS})

target_link_libraries(${CSYNC_LINK_LIBRARIES})

set_target_properties(
  ${CSYNC_LIBRARY}
    PROPERTIES
      VERSION
        0.1.0
      SOVERSION
        0
)

INSTALL(
  TARGETS
    ${CSYNC_LIBRARY}
  DESTINATION
    ${LIB_INSTALL_DIR}
)

INSTALL(
  FILES
    ${csync_HDRS}
  DESTINATION
    ${INCLUDE_INSTALL_DIR}/${APPLICATION_NAME}
)

