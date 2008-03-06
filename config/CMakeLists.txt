macro_copy_file(${CMAKE_CURRENT_SOURCE_DIR}/csync.conf ${CMAKE_CURRENT_BINARY_DIR}/csync.conf)
macro_copy_file(${CMAKE_CURRENT_SOURCE_DIR}/csync_log.conf ${CMAKE_CURRENT_BINARY_DIR}/csync_log.conf)

install(
  FILES
    csync.conf
    csync_log.conf
  DESTINATION
    ${SYSCONF_INSTALL_DIR}/csync
)
