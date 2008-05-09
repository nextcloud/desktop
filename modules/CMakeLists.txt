project(modules)

find_package(Libsmbclient REQUIRED)

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

include_directories(
  ${MODULES_PUBLIC_INCLUDE_DIRS}
  ${MODULES_PRIVATE_INCLUDE_DIRS}
)

macro_add_plugin(${SMB_PLUGIN} csync_smb.c)
target_link_libraries(${SMB_PLUGIN} ${LIBSMBCLIENT_LIBRARIES} ${CSTDLIB_LIBRARY})

macro_add_plugin(csync_dummy csync_dummy.c)

INSTALL(
  TARGETS
    ${SMB_PLUGIN}
  DESTINATION
    ${PLUGIN_INSTALL_DIR}
)

