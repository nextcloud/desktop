project(modules)

find_package(Libsmbclient REQUIRED)
find_package(LIBSSH REQUIRED)

set(MODULES_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "modules include directories"
)

set(MODULES_PRIVATE_INCLUDE_DIRS
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${LIBSMBCLIENT_INCLUDE_DIRS}
)

set(SMB_PLUGIN
  csync_smb
)

set(SFTP_PLUGIN
  csync_sftp
)

include_directories(
  ${MODULES_PUBLIC_INCLUDE_DIRS}
  ${MODULES_PRIVATE_INCLUDE_DIRS}
)

macro_add_plugin(${SMB_PLUGIN} csync_smb.c)
target_link_libraries(${SMB_PLUGIN} ${LIBSMBCLIENT_LIBRARIES} ${CSTDLIB_LIBRARY})

macro_add_plugin(csync_dummy csync_dummy.c)

macro_add_plugin(${SFTP_PLUGIN} csync_sftp.c)
target_link_libraries(${SFTP_PLUGIN} ${LIBSSH_LIBRARIES} ${CSTDLIB_LIBRARY})

INSTALL(
  TARGETS
    ${SMB_PLUGIN}
    ${SFTP_PLUGIN}
  DESTINATION
    ${PLUGIN_INSTALL_DIR}
)

# create test file as bad plugin for the vio testcase
file(WRITE
  ${CMAKE_CURRENT_BINARY_DIR}/csync_bad.so
  "bad plugin"
)
