project(modules C)

find_package(LibSSH 0.4.0)
find_package(Neon 0.29.0)

set(PLUGIN_VERSION_INSTALL_DIR "${PLUGIN_INSTALL_DIR}-${LIBRARY_SOVERSION}")

set(MODULES_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "modules include directories"
)

set(MODULES_PRIVATE_INCLUDE_DIRS
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
)

set(SMB_PLUGIN
  csync_smb
)

set(SFTP_PLUGIN
  csync_sftp
)

set(OWNCLOUD_PLUGIN
    csync_owncloud
)

include_directories(
  ${MODULES_PUBLIC_INCLUDE_DIRS}
  ${MODULES_PRIVATE_INCLUDE_DIRS}
)

find_package(SMBClient)
if(SMBCLIENT_LIBRARY)
    include_directories(${SMBCLIENT_INCLUDE_DIRS})
    macro_add_plugin(${SMB_PLUGIN} csync_smb.c)
    target_link_libraries(${SMB_PLUGIN} ${CSYNC_LIBRARY} ${SMBCLIENT_LIBRARIES})

    install(
        TARGETS
            ${SMB_PLUGIN}
        DESTINATION
            ${PLUGIN_VERSION_INSTALL_DIR}
    )
endif(SMBCLIENT_LIBRARY)

macro_add_plugin(csync_dummy csync_dummy.c)
target_link_libraries(csync_dummy ${CSYNC_LIBRARY})

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
    macro_add_plugin(${OWNCLOUD_PLUGIN} csync_owncloud.c)
    target_link_libraries(${OWNCLOUD_PLUGIN} ${CSYNC_LIBRARY} ${NEON_LIBRARY})

    install(
        TARGETS
	${OWNCLOUD_PLUGIN}
        DESTINATION
        ${PLUGIN_VERSION_INSTALL_DIR}
        )
endif (NEON_FOUND)

# create test file as bad plugin for the vio testcase
file(WRITE
  ${CMAKE_CURRENT_BINARY_DIR}/csync_bad.so
  "bad plugin"
)

