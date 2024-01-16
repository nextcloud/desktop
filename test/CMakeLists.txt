include(owncloud_add_test.cmake)

add_subdirectory(testutils)

owncloud_add_test(JHash)

owncloud_add_test(OwncloudPropagator)
owncloud_add_test(OwnSql)
owncloud_add_test(SyncJournalDB)
owncloud_add_test(SyncFileItem)
owncloud_add_test(ConcatUrl)
owncloud_add_test(XmlParse)
owncloud_add_test(ChecksumValidator)
owncloud_add_test(ConnectionValidator)


# TODO: we need keychain access for this test
if (NOT APPLE OR NOT DEFINED ENV{GITHUB_ACTION})
    owncloud_add_test(CredentialManager)
endif()

owncloud_add_test(ExcludedFiles)

owncloud_add_test(Utility)

owncloud_add_test(SyncEngine)
owncloud_add_test(SyncVirtualFiles)
owncloud_add_test(SyncMove)
add_dependencies(SyncMoveTest test_helper)
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

if (WITH_AUTO_UPDATER)
    owncloud_add_test(Updater)
endif()

owncloud_add_test(FolderMigration)

owncloud_add_test(FolderWatcher)

if( UNIX AND NOT APPLE )
    owncloud_add_test(InotifyWatcher)
endif(UNIX AND NOT APPLE)


owncloud_add_test(LongPath)

owncloud_add_test(FolderMan)

owncloud_add_test(OAuth)

configure_file(test_journal.db "${PROJECT_BINARY_DIR}/bin/test_journal.db" COPYONLY)


owncloud_add_test(JobQueue)
owncloud_add_test(SpacesMigration)

add_subdirectory(modeltests)
