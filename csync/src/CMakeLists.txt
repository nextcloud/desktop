project(libcsync)

add_subdirectory(std)
add_subdirectory(httpbf)

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
  ${HTTPBF_PUBLIC_INCLUDE_DIRS}
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
  ${NEON_LIBRARIES}
  ${HTTPBF_LIBRARY}
)

if(HAVE_ICONV AND WITH_ICONV)
    list(APPEND CSYNC_PRIVATE_INCLUDE_DIRS ${ICONV_INCLUDE_DIR})
    list(APPEND CSYNC_LINK_LIBRARIES ${ICONV_LIBRARIES})
endif()

# Specific option for builds tied to servers that do not support renaming extensions
set(NO_RENAME_EXTENSION 0 CACHE BOOL "Do not issue rename if the extension changes")
if(NO_RENAME_EXTENSION)
    add_definitions(-DNO_RENAME_EXTENSION)
endif()

set(csync_SRCS
  csync.c
  csync_exclude.c
  csync_log.c
  csync_statedb.c
  csync_time.c
  csync_util.c
  csync_misc.c

  csync_update.c
  csync_reconcile.c

  csync_rename.cc

  vio/csync_vio.c
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
if (USE_OUR_OWN_SQLITE3)
    list(APPEND csync_SRCS ${SQLITE3_SOURCE})
endif()

include_directories(
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${CSYNC_PRIVATE_INCLUDE_DIRS}
)

add_library(${CSYNC_LIBRARY} SHARED ${csync_SRCS})
#add_library(${CSYNC_LIBRARY}_static STATIC ${csync_SRCS})

target_link_libraries(${CSYNC_LIBRARY} ${CSYNC_LINK_LIBRARIES})
#target_link_libraries(${CSYNC_LIBRARY}_static ${CSYNC_LINK_LIBRARIES})

set_target_properties(
  ${CSYNC_LIBRARY}
    PROPERTIES
      VERSION
        ${LIBRARY_VERSION}
      SOVERSION
        ${LIBRARY_SOVERSION}
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

# INSTALL(
#   FILES
#     ${csync_HDRS}
#   DESTINATION
#     ${INCLUDE_INSTALL_DIR}/${APPLICATION_NAME}
# )

