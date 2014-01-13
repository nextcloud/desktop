project(libcsync)

add_subdirectory(std)
add_subdirectory(httpbf)

# Statically include sqlite
if (CSYNC_STATIC_COMPILE_DIR)
    set(SQLITE3_INCLUDE_DIRS "")
    set(SQLITE3_LIBRARIES "")
    include_directories(${CSYNC_STATIC_COMPILE_DIR})
else (CSYNC_STATIC_COMPILE_DIR)
    find_package(SQLite3 3.3.9 REQUIRED)
endif()

find_package(Neon 0.29.0 REQUIRED)


set(CSYNC_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  CACHE INTERNAL "csync public include directories"
)

set(CSYNC_PRIVATE_INCLUDE_DIRS
  ${SQLITE3_INCLUDE_DIRS}
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${HTTPBF_PUBLIC_INCLUDE_DIRS}
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
  ${SQLITE3_LIBRARIES}
  ${NEON_LIBRARIES}
  ${HTTPBF_LIBRARY}
)

if(HAVE_ICONV AND WITH_ICONV)
    list(APPEND CSYNC_PRIVATE_INCLUDE_DIRS ${ICONV_INCLUDE_DIR})
    list(APPEND CSYNC_LINK_LIBRARIES ${ICONV_LIBRARIES})
endif()

set(BLACKLIST_ON_ERROR 0 CACHE BOOL
  "If an errors occurs three times on the same file, do not attempt to process that file any further.")

if(BLACKLIST_ON_ERROR)
    add_definitions(-DBLACKLIST_ON_ERROR)
endif()

set(csync_SRCS
  csync.c
  csync_config.c
  csync_exclude.c
  csync_log.c
  csync_statedb.c
  csync_dbtree.c
  csync_time.c
  csync_util.c
  csync_misc.c
  csync_lock.c

  csync_update.c
  csync_reconcile.c

  csync_rename.cc

  vio/csync_vio.c
  vio/csync_vio_handle.c
  vio/csync_vio_file_stat.c
  vio/csync_vio_local.c

  csync_owncloud.c
  csync_owncloud_recursive_propfind.c
  csync_owncloud_util.c
)

configure_file(csync_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/csync_version.h)

set(csync_HDRS
  ${CMAKE_CURRENT_BINARY_DIR}/csync_version.h
  csync.h
  vio/csync_vio.h
  vio/csync_vio_file_stat.h
  vio/csync_vio_handle.h
  vio/csync_vio_method.h
  vio/csync_vio_module.h
)

# Statically include sqlite
if (CSYNC_STATIC_COMPILE_DIR)
    list(APPEND csync_SRCS ${CSYNC_STATIC_COMPILE_DIR}/dictionary.c  ${CSYNC_STATIC_COMPILE_DIR}/sqlite3.c)
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

