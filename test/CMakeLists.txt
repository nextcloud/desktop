include(owncloud_add_test.cmake)

owncloud_add_test(OwncloudPropagator)
owncloud_add_test(Updater)

owncloud_add_test(OwnSql)
owncloud_add_test(SyncJournalDB)
owncloud_add_test(SyncFileItem)
owncloud_add_test(ConcatUrl)
owncloud_add_test(Cookies)
owncloud_add_test(XmlParse)
owncloud_add_test(ChecksumValidator)

owncloud_add_test(ExcludedFiles)

owncloud_add_test(Utility)
owncloud_add_test(SyncEngine "syncenginetestutils.h")
owncloud_add_test(SyncVirtualFiles "syncenginetestutils.h")
owncloud_add_test(SyncMove "syncenginetestutils.h")
owncloud_add_test(SyncDelete "syncenginetestutils.h")
owncloud_add_test(SyncConflict "syncenginetestutils.h")
owncloud_add_test(SyncFileStatusTracker "syncenginetestutils.h")
owncloud_add_test(Download "syncenginetestutils.h")
owncloud_add_test(ChunkingNg "syncenginetestutils.h")
owncloud_add_test(AsyncOp "syncenginetestutils.h")
owncloud_add_test(UploadReset "syncenginetestutils.h")
owncloud_add_test(AllFilesDeleted "syncenginetestutils.h")
owncloud_add_test(Blacklist "syncenginetestutils.h")
owncloud_add_test(LocalDiscovery "syncenginetestutils.h")
owncloud_add_test(RemoteDiscovery "syncenginetestutils.h")
owncloud_add_test(Permissions "syncenginetestutils.h")
owncloud_add_test(SelectiveSync "syncenginetestutils.h")
owncloud_add_test(DatabaseError "syncenginetestutils.h")
# For unknown reasons the DatabaseErrorTest occasionally aborts during drone execution
set_tests_properties(DatabaseErrorTest PROPERTIES LABELS "nodrone" )

owncloud_add_test(LockedFiles "syncenginetestutils.h")

owncloud_add_test(FolderWatcher)

if( UNIX AND NOT APPLE )
    owncloud_add_test(InotifyWatcher)
endif(UNIX AND NOT APPLE)

owncloud_add_benchmark(LargeSync "syncenginetestutils.h")

owncloud_add_test(FolderMan)

owncloud_add_test(OAuth "syncenginetestutils.h")

configure_file(test_journal.db "${PROJECT_BINARY_DIR}/bin/test_journal.db" COPYONLY)

find_package(CMocka)
if (CMOCKA_FOUND)
    include(AddCMockaTest)
    add_subdirectory(csync)
endif (CMOCKA_FOUND)
