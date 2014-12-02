include_directories(${CMAKE_BINARY_DIR}/csync ${CMAKE_BINARY_DIR}/csync/src ${CMAKE_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/csync/src/ ${CMAKE_SOURCE_DIR}/csync/src/httpbf/src)
include_directories(${CMAKE_SOURCE_DIR}/csync/src/std ${CMAKE_SOURCE_DIR}/src)

include(owncloud_add_test.cmake)

owncloud_add_test(OwncloudPropagator "")
owncloud_add_test(Utility "")
owncloud_add_test(Updater "")

SET(FolderWatcher_SRC ../src/gui/folderwatcher.cpp)

IF( NOT WIN32 AND NOT APPLE )
list(APPEND FolderWatcher_SRC  ../src/gui/folderwatcher_linux.cpp)
ENDIF()
IF( WIN32 )
list(APPEND  FolderWatcher_SRC ../src/gui/folderwatcher_win.cpp)
ENDIF()
IF( APPLE )
list(APPEND FolderWatcher_SRC ../src/gui/folderwatcher_mac.cpp)
ENDIF()

owncloud_add_test(FolderWatcher "${FolderWatcher_SRC}")
if( UNIX AND NOT APPLE )
    owncloud_add_test(InotifyWatcher "${FolderWatcher_SRC}")
endif(UNIX AND NOT APPLE)

owncloud_add_test(CSyncSqlite "")
owncloud_add_test(NetrcParser ../src/cmd/netrcparser.cpp)
owncloud_add_test(OwnSql "")
owncloud_add_test(SyncJournalDB "")



