include_directories(${CMAKE_CURRENT_LIST_DIR}/../src)
include_directories(${CSYNC_INCLUDE_DIR}/csync ${CSYNC_INCLUDE_DIR} ${CSYNC_INCLUDE_DIR}/httpbf/src ${CSYNC_BUILD_PATH}/src)
include(owncloud_add_test.cmake)

owncloud_add_test(OwncloudPropagator)
owncloud_add_test(Utility)
