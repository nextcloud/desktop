include(DefinePlatformDefaults)
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

include(nextcloud_add_test.cmake)

set(CMAKE_AUTOMOC TRUE)
add_library(syncenginetestutils STATIC syncenginetestutils.cpp)
target_link_libraries(syncenginetestutils PUBLIC ${APPLICATION_EXECUTABLE}sync Qt5::Test)

nextcloud_add_test(NextcloudPropagator "")

IF(BUILD_UPDATER)
    nextcloud_add_test(Updater "")
endif()

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
nextcloud_add_test(NetrcParser ../src/cmd/netrcparser.cpp)
nextcloud_add_test(OwnSql "")
nextcloud_add_test(SyncJournalDB "")
nextcloud_add_test(SyncFileItem "")
nextcloud_add_test(ConcatUrl "")
nextcloud_add_test(Cookies "")
nextcloud_add_test(XmlParse "")
nextcloud_add_test(ChecksumValidator "")

nextcloud_add_test(ClientSideEncryption "")
nextcloud_add_test(ExcludedFiles "")

nextcloud_add_test(Utility "")
nextcloud_add_test(SyncEngine "")
nextcloud_add_test(SyncVirtualFiles "")
nextcloud_add_test(SyncMove "")
nextcloud_add_test(SyncDelete "")
nextcloud_add_test(SyncConflict "")
nextcloud_add_test(SyncFileStatusTracker "")
nextcloud_add_test(Download "")
nextcloud_add_test(ChunkingNg "")
nextcloud_add_test(AsyncOp "")
nextcloud_add_test(UploadReset "")
nextcloud_add_test(AllFilesDeleted "")
nextcloud_add_test(Blacklist "")
nextcloud_add_test(LocalDiscovery "")
nextcloud_add_test(RemoteDiscovery "")
nextcloud_add_test(Permissions "")
nextcloud_add_test(SelectiveSync "")
nextcloud_add_test(DatabaseError "")
nextcloud_add_test(LockedFiles "../src/gui/lockwatcher.cpp")
nextcloud_add_test(FolderWatcher "${FolderWatcher_SRC}")

if( UNIX AND NOT APPLE )
    nextcloud_add_test(InotifyWatcher "${FolderWatcher_SRC}")
endif(UNIX AND NOT APPLE)

if (WIN32)
    nextcloud_add_test(LongWinPath "")
    nextcloud_add_test(SyncCfApi "")
elseif(LINUX) # elseif(LINUX OR APPLE)
    nextcloud_add_test(SyncXAttr "")
endif()

nextcloud_add_benchmark(LargeSync "")

SET(FolderMan_SRC ../src/gui/folderman.cpp)
list(APPEND FolderMan_SRC ../src/gui/folder.cpp )
list(APPEND FolderMan_SRC ../src/gui/conflictsolver.cpp )
list(APPEND FolderMan_SRC ../src/gui/socketapi.cpp )
list(APPEND FolderMan_SRC ../src/gui/syncrunfilelog.cpp )
list(APPEND FolderMan_SRC ../src/gui/lockwatcher.cpp )
list(APPEND FolderMan_SRC ../src/gui/guiutility.cpp )
list(APPEND FolderMan_SRC ../src/gui/navigationpanehelper.cpp )
list(APPEND FolderMan_SRC ../src/gui/userinfo.cpp )
list(APPEND FolderMan_SRC ../src/gui/connectionvalidator.cpp )
list(APPEND FolderMan_SRC ../src/gui/ocsjob.cpp )
list(APPEND FolderMan_SRC ../src/gui/ocsnavigationappsjob.cpp )
list(APPEND FolderMan_SRC ../src/gui/accountstate.cpp )
list(APPEND FolderMan_SRC ../src/gui/remotewipe.cpp )
list(APPEND FolderMan_SRC ${FolderWatcher_SRC})
list(APPEND FolderMan_SRC stubfolderman.cpp )
nextcloud_add_test(FolderMan "${FolderMan_SRC}")

SET(RemoteWipe_SRC ../src/gui/remotewipe.cpp)
list(APPEND RemoteWipe_SRC ../src/gui/guiutility.cpp )
list(APPEND RemoteWipe_SRC ../src/gui/userinfo.cpp )
list(APPEND RemoteWipe_SRC ../src/gui/connectionvalidator.cpp )
list(APPEND RemoteWipe_SRC ../src/gui/ocsjob.cpp )
list(APPEND RemoteWipe_SRC ../src/gui/ocsnavigationappsjob.cpp )
list(APPEND RemoteWipe_SRC ../src/gui/accountstate.cpp )
list(APPEND RemoteWipe_SRC ../src/gui/conflictsolver.cpp )
list(APPEND RemoteWipe_SRC ../src/gui/socketapi.cpp )
list(APPEND RemoteWipe_SRC ../src/gui/folder.cpp )
list(APPEND RemoteWipe_SRC ../src/gui/syncrunfilelog.cpp )
list(APPEND RemoteWipe_SRC ${FolderWatcher_SRC} )
list(APPEND RemoteWipe_SRC ../src/gui/folderwatcher.cpp )
list(APPEND RemoteWipe_SRC ${RemoteWipe_SRC})
list(APPEND RemoteWipe_SRC stubremotewipe.cpp )
nextcloud_add_test(RemoteWipe "${RemoteWipe_SRC}")

nextcloud_add_test(OAuth "../src/gui/creds/oauth.cpp")

configure_file(test_journal.db "${PROJECT_BINARY_DIR}/bin/test_journal.db" COPYONLY)

find_package(CMocka)
if (CMOCKA_FOUND)
    include(AddCMockaTest)
    add_subdirectory(csync)
endif (CMOCKA_FOUND)
