macro_copy_file(${CMAKE_CURRENT_SOURCE_DIR}/ocsync.conf ${CMAKE_CURRENT_BINARY_DIR}/ocsync.conf)
macro_copy_file(${CMAKE_CURRENT_SOURCE_DIR}/ocsync_exclude.conf ${CMAKE_CURRENT_BINARY_DIR}/ocsync_exclude.conf)

install(
  FILES
    ocsync.conf
    ocsync_exclude.conf
  DESTINATION
    ${SYSCONF_INSTALL_DIR}/ocsync
)
