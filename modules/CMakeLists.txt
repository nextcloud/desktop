project(modules C)

find_package(Libsmbclient)
find_package(LibSSH 0.4.0)
find_package(Neon)

set(PLUGIN_VERSION_INSTALL_DIR "${PLUGIN_INSTALL_DIR}-${LIBRARY_SOVERSION}")

set(MODULES_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "modules include directories"
)

set(MODULES_PRIVATE_INCLUDE_DIRS
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${HTTPBF_PUBLIC_INCLUDE_DIRS}
)

set(SMB_PLUGIN
  ocsync_smb
)

set(SFTP_PLUGIN
  ocsync_sftp
)

set(OWNCLOUD_PLUGIN
  ocsync_owncloud
)

include_directories(
  ${MODULES_PUBLIC_INCLUDE_DIRS}
  ${MODULES_PRIVATE_INCLUDE_DIRS}
)

if(LIBSMBCLIENT_FOUND)
include_directories(${LIBSMBCLIENT_INCLUDE_DIRS})
macro_add_plugin(${SMB_PLUGIN} csync_smb.c)
target_link_libraries(${SMB_PLUGIN} ${CSYNC_LIBRARY} ${LIBSMBCLIENT_LIBRARIES})

install(
  TARGETS
    ${SMB_PLUGIN}
  DESTINATION
    ${PLUGIN_VERSION_INSTALL_DIR}
)
endif()

macro_add_plugin(ocsync_dummy csync_dummy.c)
target_link_libraries(ocsync_dummy ${CSYNC_LIBRARY})

if (LIBSSH_FOUND)
macro_add_plugin(${SFTP_PLUGIN} csync_sftp.c)
target_link_libraries(${SFTP_PLUGIN} ${CSYNC_LIBRARY} ${LIBSSH_LIBRARIES})

install(
  TARGETS
    ${SFTP_PLUGIN}
  DESTINATION
    ${PLUGIN_VERSION_INSTALL_DIR}
)
endif (LIBSSH_FOUND)

if (NEON_FOUND)
    include_directories( ${NEON_INCLUDE_DIRS} )
    macro_add_plugin(${OWNCLOUD_PLUGIN} csync_owncloud.c csync_owncloud_recursive_propfind.c csync_owncloud_util.c)
    target_link_libraries(${OWNCLOUD_PLUGIN} ${CSYNC_LIBRARY} ${NEON_LIBRARIES} ${HTTPBF_LIBRARY})

    install(
        TARGETS
	${OWNCLOUD_PLUGIN}
        DESTINATION
        ${PLUGIN_VERSION_INSTALL_DIR}
        )
endif (NEON_FOUND)

# create test file as bad plugin for the vio testcase
file(WRITE
  ${CMAKE_CURRENT_BINARY_DIR}/ocsync_bad.so
  "bad plugin"
)

