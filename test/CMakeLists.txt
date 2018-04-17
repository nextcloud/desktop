find_package(SQLite3 3.8.0 REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/src
                    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer
                    ${CMAKE_SOURCE_DIR}/src/csync
                    ${CMAKE_SOURCE_DIR}/src/csync/std
                    ${CMAKE_SOURCE_DIR}/src/gui
                    ${CMAKE_SOURCE_DIR}/src/libsync
                    ${CMAKE_BINARY_DIR}/src/csync
                    ${CMAKE_BINARY_DIR}/src/libsync
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR}
                    ${SQLITE3_INCLUDE_DIR}
                   )

include(owncloud_add_test.cmake)

owncloud_add_test(OwncloudPropagator "")
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
list(APPEND FolderWatcher_SRC ../src/gui/socketapisocket_mac.mm)
ENDIF()
owncloud_add_test(NetrcParser ../src/cmd/netrcparser.cpp)
owncloud_add_test(OwnSql "")
owncloud_add_test(SyncJournalDB "")
owncloud_add_test(SyncFileItem "")
owncloud_add_test(ConcatUrl "")
owncloud_add_test(XmlParse "")
owncloud_add_test(ChecksumValidator "")

owncloud_add_test(ExcludedFiles "")

owncloud_add_test(FileSystem "")
owncloud_add_test(Utility "")
owncloud_add_test(SyncEngine "syncenginetestutils.h")
owncloud_add_test(SyncMove "syncenginetestutils.h")
owncloud_add_test(SyncConflict "syncenginetestutils.h")
owncloud_add_test(SyncFileStatusTracker "syncenginetestutils.h")
owncloud_add_test(Download "syncenginetestutils.h")
owncloud_add_test(ChunkingNg "syncenginetestutils.h")
owncloud_add_test(UploadReset "syncenginetestutils.h")
owncloud_add_test(AllFilesDeleted "syncenginetestutils.h")
owncloud_add_test(Blacklist "syncenginetestutils.h")
owncloud_add_test(FolderWatcher "${FolderWatcher_SRC}")

if( UNIX AND NOT APPLE )
    owncloud_add_test(InotifyWatcher "${FolderWatcher_SRC}")
endif(UNIX AND NOT APPLE)

owncloud_add_benchmark(LargeSync "syncenginetestutils.h")

SET(FolderMan_SRC ../src/gui/folderman.cpp)
list(APPEND FolderMan_SRC ../src/gui/folder.cpp )
list(APPEND FolderMan_SRC ../src/gui/socketapi.cpp )
list(APPEND FolderMan_SRC ../src/gui/accountstate.cpp )
list(APPEND FolderMan_SRC ../src/gui/syncrunfilelog.cpp )
list(APPEND FolderMan_SRC ../src/gui/lockwatcher.cpp )
list(APPEND FolderMan_SRC ../src/gui/guiutility.cpp )
list(APPEND FolderMan_SRC ../src/gui/navigationpanehelper.cpp )
list(APPEND FolderMan_SRC ../src/gui/connectionvalidator.cpp )
list(APPEND FolderMan_SRC ../src/gui/clientproxy.cpp )
list(APPEND FolderMan_SRC ${FolderWatcher_SRC})
list(APPEND FolderMan_SRC stub.cpp )
owncloud_add_test(FolderMan "${FolderMan_SRC}")

owncloud_add_test(OAuth "syncenginetestutils.h;../src/gui/creds/oauth.cpp")

configure_file(test_journal.db "${PROJECT_BINARY_DIR}/bin/test_journal.db" COPYONLY)

find_package(CMocka)
if (CMOCKA_FOUND)
    include(AddCMockaTest)
    add_subdirectory(csync)
endif (CMOCKA_FOUND)
