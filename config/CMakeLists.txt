macro_copy_file(${CMAKE_CURRENT_SOURCE_DIR}/csync.conf ${CMAKE_CURRENT_BINARY_DIR}/csync.conf)
macro_copy_file(${CMAKE_CURRENT_SOURCE_DIR}/csync_exclude.conf ${CMAKE_CURRENT_BINARY_DIR}/csync_exclude.conf)

install(
  FILES
    csync.conf
    csync_exclude.conf
  DESTINATION
    ${SYSCONF_INSTALL_DIR}/csync
)
