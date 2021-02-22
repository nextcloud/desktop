include(owncloud_add_test.cmake)

add_library(syncenginetestutils STATIC syncenginetestutils.cpp)
target_link_libraries(syncenginetestutils PUBLIC owncloudCore Qt5::Test)

owncloud_add_test(OwncloudPropagator)
owncloud_add_test(Updater)

owncloud_add_test(OwnSql)
owncloud_add_test(SyncJournalDB)
owncloud_add_test(SyncFileItem)
owncloud_add_test(ConcatUrl)
owncloud_add_test(Cookies)
owncloud_add_test(XmlParse)
owncloud_add_test(ChecksumValidator)
owncloud_add_test(CredentialManager)

owncloud_add_test(ExcludedFiles)

owncloud_add_test(Utility)
owncloud_add_test(SyncEngine)
owncloud_add_test(SyncVirtualFiles)
owncloud_add_test(SyncMove)
owncloud_add_test(SyncDelete)
owncloud_add_test(SyncConflict)
owncloud_add_test(SyncFileStatusTracker)
owncloud_add_test(Download)
owncloud_add_test(ChunkingNg)
owncloud_add_test(UploadReset)
owncloud_add_test(AllFilesDeleted)
owncloud_add_test(Blacklist)
owncloud_add_test(LocalDiscovery)
owncloud_add_test(RemoteDiscovery)
owncloud_add_test(Permissions)
owncloud_add_test(SelectiveSync)
owncloud_add_test(DatabaseError)
owncloud_add_test(LockedFiles)

owncloud_add_test(FolderWatcher)

if( UNIX AND NOT APPLE )
    owncloud_add_test(InotifyWatcher)
endif(UNIX AND NOT APPLE)


owncloud_add_test(LongPath)
owncloud_add_benchmark(LargeSync)

owncloud_add_test(FolderMan)

owncloud_add_test(OAuth)

configure_file(test_journal.db "${PROJECT_BINARY_DIR}/bin/test_journal.db" COPYONLY)


owncloud_add_test(JobQueue)

find_package(CMocka)
if (CMOCKA_FOUND)
    include(AddCMockaTest)
    add_subdirectory(csync)
endif (CMOCKA_FOUND)
