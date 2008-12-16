project(libcsync)

add_subdirectory(std)

find_package(SQLite3 REQUIRED)
find_package(Iniparser REQUIRED)
find_package(Dlfcn REQUIRED)
find_package(RT REQUIRED)

set(CSYNC_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  CACHE INTERNAL "csync public include directories"
)

set(CSYNC_PRIVATE_INCLUDE_DIRS
  ${DLFCN_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${INIPARSER_INCLUDE_DIRS}
  ${LOG4C_INCLUDE_DIRS}
  ${SQLITE3_INCLUDE_DIRS}
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
  ${DLFCN_LIBRARIES}
  ${RT_LIBRARIES}
  ${INIPARSER_LIBRARIES}
  ${LOG4C_LIBRARIES}
  ${SQLITE3_LIBRARIES}
)

set(csync_SRCS
  csync.c
  csync_config.c
  csync_exclude.c
  csync_statedb.c
  csync_lock.c
  csync_time.c
  csync_util.c

  csync_update.c
  csync_reconcile.c
  csync_propagate.c

  vio/csync_vio.c
  vio/csync_vio_handle.c
  vio/csync_vio_file_stat.c
  vio/csync_vio_local.c
)

set(csync_HDRS
  csync.h
  vio/csync_vio.h
  vio/csync_vio_file_stat.h
  vio/csync_vio_handle.h
  vio/csync_vio_method.h
  vio/csync_vio_module.h
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
        ${LIBRARY_VERSION}
      SOVERSION
        ${LIBRARY_SOVERSION}
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

