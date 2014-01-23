include_directories(${CMAKE_BINARY_DIR}/csync ${CMAKE_BINARY_DIR}/csync/src ${CMAKE_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/csync/src/ ${CMAKE_SOURCE_DIR}/csync/src/httpbf/src)
include(owncloud_add_test.cmake)

owncloud_add_test(OwncloudPropagator)
owncloud_add_test(Utility)

if( UNIX AND NOT APPLE )
	owncloud_add_test(InotifyWatcher)
endif(UNIX AND NOT APPLE)

owncloud_add_test(FolderWatcher)
