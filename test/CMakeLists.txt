# SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2011 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

include(DefinePlatformDefaults)

include(nextcloud_add_test.cmake)

set(CMAKE_AUTOMOC TRUE)

find_package(Qt${QT_VERSION_MAJOR}Core5Compat ${REQUIRED_QT_VERSION} CONFIG QUIET)
find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS HttpServer CONFIG QUIET)

add_library(testutils
  STATIC
  syncenginetestutils.cpp
  pushnotificationstestutils.cpp
  themeutils.cpp
  testhelper.cpp
  sharetestutils.cpp
  endtoendtestutils.cpp
  activitylistmodeltestutils.cpp
  foldermantestutils.cpp
)

target_link_libraries(testutils PUBLIC Nextcloud::sync Qt::Test Qt::Core5Compat)
target_include_directories(testutils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(testutils PROPERTIES FOLDER Tests)

nextcloud_add_test(NextcloudPropagator)
if(Qt${QT_VERSION_MAJOR}HttpServer_FOUND)
    target_compile_definitions(NextcloudPropagatorTest PRIVATE HAVE_QHTTPSERVER=1)
    target_link_libraries(NextcloudPropagatorTest PRIVATE Qt::HttpServer)
endif()

IF(BUILD_UPDATER)
    nextcloud_add_test(Updater)

    if(Qt${QT_VERSION_MAJOR}HttpServer_FOUND)
        target_compile_definitions(UpdaterTest PRIVATE HAVE_QHTTPSERVER=1)
        target_link_libraries(UpdaterTest PRIVATE Qt::HttpServer)
    endif()
endif()

nextcloud_add_test(NetrcParser)
nextcloud_add_test(OwnSql)
nextcloud_add_test(SyncJournalDB)
nextcloud_add_test(SyncFileItem)
nextcloud_add_test(ConcatUrl)
nextcloud_add_test(Cookies)
nextcloud_add_test(XmlParse)
nextcloud_add_test(ChecksumValidator)

nextcloud_add_test(ClientSideEncryption)
nextcloud_add_test(ClientSideEncryptionV2)

if (NOT APPLE)
    nextcloud_add_test(ExcludedFiles)
endif()

nextcloud_add_test(Utility)

if (NOT APPLE)
    nextcloud_add_test(SyncEngine)
    nextcloud_add_test(SyncVirtualFiles)
endif()

nextcloud_add_test(SyncMove)
nextcloud_add_test(SyncDelete)
nextcloud_add_test(SyncConflict)
nextcloud_add_test(SyncFileStatusTracker)
nextcloud_add_test(Download)
nextcloud_add_test(ChunkingNg)
nextcloud_add_test(AsyncOp)
nextcloud_add_test(UploadReset)
nextcloud_add_test(AllFilesDeleted)
nextcloud_add_test(Blacklist)
nextcloud_add_test(LocalDiscovery)
nextcloud_add_test(RemoteDiscovery)

if (NOT APPLE)
    nextcloud_add_test(Permissions)
endif()

nextcloud_add_test(SelectiveSync)
nextcloud_add_test(DatabaseError)
nextcloud_add_test(LockedFiles)

if (NOT WIN32)
    nextcloud_add_test(FolderWatcher)
endif()

nextcloud_add_test(Capabilities)
nextcloud_add_test(PushNotifications)
nextcloud_add_test(Theme)
nextcloud_add_test(IconUtils)
nextcloud_add_test(SetUserStatusDialog)
nextcloud_add_test(UnifiedSearchListmodel)
nextcloud_add_test(ActivityListModel)
nextcloud_add_test(SortedActivityListModel)
nextcloud_add_test(ActivityData)
nextcloud_add_test(TalkReply)
nextcloud_add_test(LockFile)
nextcloud_add_test(ShareModel)
nextcloud_add_test(ShareeModel)
nextcloud_add_test(SortedShareModel)
nextcloud_add_test(SecureFileDrop)
nextcloud_add_test(FileTagModel)
nextcloud_add_test(SyncConflictsModel)
nextcloud_add_test(DateFieldBackend)
nextcloud_add_test(ClientStatusReporting)

nextcloud_add_test(FileSystem)

nextcloud_add_test(FolderStatusModel)

target_link_libraries(SecureFileDropTest PRIVATE Nextcloud::sync)
configure_file(fake2eelocksucceeded.json "${PROJECT_BINARY_DIR}/bin/fake2eelocksucceeded.json" COPYONLY)
configure_file(fakefiledrope2eefoldermetadata.json "${PROJECT_BINARY_DIR}/bin/fakefiledrope2eefoldermetadata.json" COPYONLY)
configure_file(e2etestsfakecert.pem "${PROJECT_BINARY_DIR}/bin/e2etestsfakecert.pem" COPYONLY)
configure_file(e2etestsfakecertprivatekey.pem "${PROJECT_BINARY_DIR}/bin/e2etestsfakecertprivatekey.pem" COPYONLY)
configure_file(e2etestsfakecertpublickey.pem "${PROJECT_BINARY_DIR}/bin/e2etestsfakecertpublickey.pem" COPYONLY)
configure_file(fakeshortcut.base64 "${PROJECT_BINARY_DIR}/bin/fakeshortcut.base64" COPYONLY)


if(ADD_E2E_TESTS)
    nextcloud_add_test(E2eServerSetup)
    nextcloud_add_test(E2eFileTransfer)
else()
    nextcloud_build_test(E2eServerSetup)
    nextcloud_build_test(E2eFileTransfer)
endif()

if( UNIX AND NOT APPLE )
    nextcloud_add_test(InotifyWatcher)
endif(UNIX AND NOT APPLE)

if (WIN32)
    include_directories(
        ${CMAKE_BINARY_DIR}/src/libsync/vfs/cfapi
    )

    nextcloud_add_test(SyncCfApi)
	nextcloud_add_test(CfApiShellExtensionsIPC)
    target_sources(CfApiShellExtensionsIPCTest PRIVATE "${CMAKE_SOURCE_DIR}/src/libsync/vfs/cfapi/shellext/thumbnailprovideripc.cpp" "${CMAKE_SOURCE_DIR}/src/libsync/vfs/cfapi/shellext/customstateprovideripc.cpp" "${CMAKE_SOURCE_DIR}/src/libsync/vfs/cfapi/shellext/ipccommon.cpp")
elseif(LINUX) # elseif(LINUX OR APPLE)
    nextcloud_add_test(SyncXAttr)
endif()

nextcloud_add_test(LongPath)
nextcloud_add_benchmark(LargeSync)

nextcloud_add_test(Account)
nextcloud_add_test(Folder)
nextcloud_add_test(FolderMan)
nextcloud_add_test(RemoteWipe)

if(NOT BUILD_FILE_PROVIDER_MODULE)
    # the File Provider build crashes this test in CI for some reason
    # I'm not yet sure what's causing it as it works fine with a classic build...
    nextcloud_add_test(AccountSettings)
endif()

nextcloud_add_test(NetworkSettings)
nextcloud_add_test(UpdateChannel)
nextcloud_add_test(FileActionsModel)

configure_file(test_journal.db "${PROJECT_BINARY_DIR}/bin/test_journal.db" COPYONLY)

find_package(CMocka)
if (CMOCKA_FOUND)
    include(AddCMockaTest)
    add_subdirectory(csync)
endif (CMOCKA_FOUND)
