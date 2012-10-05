project(doc C)
# Build the documentation
#

include(UseDoxygen OPTIONAL)

file(GLOB _manpages *.[0-9].txt)
add_custom_target(man
  COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/makeman.sh ${_manpages}
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(userguide
  COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/makeguide.sh ocsync.txt
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}
)

install(
  FILES
    ocsync.1
  DESTINATION
    ${MAN_INSTALL_DIR}/man1
)

install(
  DIRECTORY
    userguide
  DESTINATION
    ${SHARE_INSTALL_PREFIX}/doc/ocsync
)
