include(DefinePlatformDefaults)
find_package(SQLite3 3.8.0 REQUIRED)

include(nextcloud_add_test.cmake)

set(CMAKE_AUTOMOC TRUE)

add_library(syncenginetestutils STATIC syncenginetestutils.cpp)
target_link_libraries(syncenginetestutils PUBLIC ${APPLICATION_EXECUTABLE}sync Qt5::Test)
target_include_directories(syncenginetestutils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

nextcloud_add_test(NextcloudPropagator "")

IF(BUILD_UPDATER)
    nextcloud_add_test(Updater "")
endif()

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
nextcloud_add_test(FolderWatcher "")
nextcloud_add_test(Capabilities "")
nextcloud_add_test(PushNotifications "pushnotificationstestutils.cpp")

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

nextcloud_add_test(FolderMan "")
nextcloud_add_test(RemoteWipe "")

nextcloud_add_test(OAuth "../src/gui/creds/oauth.cpp")

configure_file(test_journal.db "${PROJECT_BINARY_DIR}/bin/test_journal.db" COPYONLY)

find_package(CMocka)
if (CMOCKA_FOUND)
    include(AddCMockaTest)
    add_subdirectory(csync)
endif (CMOCKA_FOUND)
