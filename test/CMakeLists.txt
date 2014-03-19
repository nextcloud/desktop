include_directories(${CMAKE_BINARY_DIR}/csync ${CMAKE_BINARY_DIR}/csync/src ${CMAKE_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/csync/src/ ${CMAKE_SOURCE_DIR}/csync/src/httpbf/src)
include_directories(${CMAKE_SOURCE_DIR}/csync/src/std)

include(owncloud_add_test.cmake)

owncloud_add_test(OwncloudPropagator)
owncloud_add_test(Utility)
owncloud_add_test(Updater)
owncloud_add_test(FolderWatcher)
owncloud_add_test(CSyncSqlite)

if( UNIX AND NOT APPLE )
	owncloud_add_test(InotifyWatcher)
endif(UNIX AND NOT APPLE)

