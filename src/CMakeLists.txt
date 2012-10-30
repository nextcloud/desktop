project(libcsync C)

add_subdirectory(std)

# Statically include sqlite and iniparser
if (CSYNC_STATIC_COMPILE_DIR)
    set(SQLITE3_INCLUDE_DIRS "")
    set(SQLITE3_LIBRARIES "")
    set(INIPARSER_LIBRARIES "")
    set(INIPARSER_INCLUDE_DIRS "")
    include_directories(${CSYNC_STATIC_COMPILE_DIR})
else (CSYNC_STATIC_COMPILE_DIR)
    find_package(SQLite3 REQUIRED)
    find_package(Iniparser REQUIRED)
endif()


set(CSYNC_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  CACHE INTERNAL "csync public include directories"
)

set(CSYNC_PRIVATE_INCLUDE_DIRS
  ${INIPARSER_INCLUDE_DIRS}
  ${SQLITE3_INCLUDE_DIRS}
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
)

set(CSYNC_LIBRARY
  ocsync
  CACHE INTERNAL "ocsync library"
)

set(CSYNC_LINK_LIBRARIES
  ${CSYNC_LIBRARY}
  ${CSTDLIB_LIBRARY}
  ${CSYNC_REQUIRED_LIBRARIES}
  ${INIPARSER_LIBRARIES}
  ${SQLITE3_LIBRARIES}
)

if(LOG4C_FOUND)
    list(APPEND CSYNC_PRIVATE_INCLUDE_DIRS ${LOG4C_INCLUDE_DIRS})
    list(APPEND CSYNC_LINK_LIBRARIES ${LOG4C_LIBRARIES})
endif()

set(csync_SRCS
  csync.c
  csync_config.c
  csync_exclude.c
  csync_statedb.c
  csync_dbtree.c
  csync_time.c
  csync_util.c
  csync_misc.c

  csync_update.c
  csync_reconcile.c
  csync_propagate.c

  vio/csync_vio.c
  vio/csync_vio_handle.c
  vio/csync_vio_file_stat.c
  vio/csync_vio_local.c
)

if(NOT WIN32)
  list(APPEND csync_SRCS csync_lock.c)
endif()

set(csync_HDRS
  csync.h
  vio/csync_vio.h
  vio/csync_vio_file_stat.h
  vio/csync_vio_handle.h
  vio/csync_vio_method.h
  vio/csync_vio_module.h
)

# Statically include sqlite and iniparser
if (CSYNC_STATIC_COMPILE_DIR)
    list(APPEND csync_SRCS ${CSYNC_STATIC_COMPILE_DIR}/dictionary.c ${CSYNC_STATIC_COMPILE_DIR}/iniparser.c ${CSYNC_STATIC_COMPILE_DIR}/sqlite3.c)
endif()

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
  LIBRARY DESTINATION
    ${LIB_INSTALL_DIR}
  ARCHIVE DESTINATION
    ${LIB_INSTALL_DIR}
  RUNTIME DESTINATION
    ${BIN_INSTALL_DIR}
)

INSTALL(
  FILES
    ${csync_HDRS}
  DESTINATION
    ${INCLUDE_INSTALL_DIR}/${APPLICATION_NAME}
)

