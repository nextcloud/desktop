//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
@testable import TestInterface
import XCTest

final class EnumeratorTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    var rootItem: MockRemoteItem!
    var remoteFolder: MockRemoteItem!
    var remoteItemA: MockRemoteItem!
    var remoteItemB: MockRemoteItem!
    var remoteItemC: MockRemoteItem!

    var rootTrashItem: MockRemoteItem!
    var remoteTrashItemA: MockRemoteItem!
    var remoteTrashItemB: MockRemoteItem!
    var remoteTrashItemC: MockRemoteItem!

    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name

        rootItem = MockRemoteItem.rootItem(account: Self.account)

        remoteFolder = MockRemoteItem(
            identifier: "folder",
            versionIdentifier: "NEW",
            name: "folder",
            remotePath: Self.account.davFilesUrl + "/folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteItemA = MockRemoteItem(
            identifier: "itemA",
            versionIdentifier: "NEW",
            name: "itemA",
            remotePath: Self.account.davFilesUrl + "/folder/itemA",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteItemB = MockRemoteItem(
            identifier: "itemB",
            name: "itemB",
            remotePath: Self.account.davFilesUrl + "/folder/itemB",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteItemC = MockRemoteItem(
            identifier: "itemC",
            name: "itemC",
            remotePath: Self.account.davFilesUrl + "/folder/itemC",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItemA, remoteItemB]
        remoteItemA.parent = remoteFolder
        remoteItemB.parent = remoteFolder
        remoteItemC.parent = nil

        rootTrashItem = MockRemoteItem.rootTrashItem(account: Self.account)

        remoteTrashItemA = MockRemoteItem(
            identifier: "trashItemA",
            name: "a.txt",
            remotePath: Self.account.trashUrl + "/a.txt",
            data: Data(repeating: 1, count: 32),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteTrashItemB = MockRemoteItem(
            identifier: "trashItemB",
            name: "b.txt",
            remotePath: Self.account.trashUrl + "/b.txt",
            data: Data(repeating: 1, count: 69),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteTrashItemC = MockRemoteItem(
            identifier: "trashItemC",
            name: "c.txt",
            remotePath: Self.account.trashUrl + "/c.txt",
            data: Data(repeating: 1, count: 100),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        rootTrashItem.children = [remoteTrashItemA, remoteTrashItemB, remoteTrashItemC]
        remoteTrashItemA.parent = rootTrashItem
        remoteTrashItemB.parent = rootTrashItem
        remoteTrashItemC.parent = rootTrashItem
    }

    func testRootEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 2)

        let retrievedRootItem = try XCTUnwrap(observer.items.first)
        XCTAssertEqual(retrievedRootItem.itemIdentifier.rawValue, rootItem.identifier)

        let retrievedFolderItem = try XCTUnwrap(observer.items.last)
        XCTAssertEqual(retrievedFolderItem.itemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedFolderItem.filename, remoteFolder.name)
        XCTAssertEqual(retrievedFolderItem.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(retrievedFolderItem.creationDate, remoteFolder.creationDate)
        XCTAssertEqual(
            Int(retrievedFolderItem.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteFolder.modificationDate.timeIntervalSince1970)
        )

        let dbFolder = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertEqual(dbFolder.etag, remoteFolder.versionIdentifier)
        XCTAssertEqual(dbFolder.fileName, remoteFolder.name)
        XCTAssertEqual(dbFolder.fileNameView, remoteFolder.name)
        XCTAssertEqual(dbFolder.serverUrl + "/" + dbFolder.fileName, remoteFolder.remotePath)
        XCTAssertEqual(dbFolder.account, Self.account.ncKitAccount)
        XCTAssertEqual(dbFolder.user, Self.account.username)
        XCTAssertEqual(dbFolder.userId, Self.account.id)
        XCTAssertEqual(dbFolder.urlBase, Self.account.serverUrl)

        let storedFolderItemMaybe = await Item.storedItem(
            identifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let storedFolderItem = try XCTUnwrap(storedFolderItemMaybe)
        XCTAssertEqual(storedFolderItem.itemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(storedFolderItem.filename, remoteFolder.name)
        XCTAssertEqual(storedFolderItem.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(storedFolderItem.creationDate, remoteFolder.creationDate)
        XCTAssertEqual(
            Int(storedFolderItem.contentModificationDate?.timeIntervalSince1970 ?? 0),
            Int(remoteFolder.modificationDate.timeIntervalSince1970)
        )
        XCTAssertEqual(storedFolderItem.childItemCount?.intValue, 0) // Not visited yet, so no kids
    }

    func testWorkingSetEnumeration() async throws {
        // This test verifies that the working set enumerator correctly returns ONLY the
        // items that are "materialised" (downloaded files or visited directories) from the database
        let db = Self.dbManager.ncDatabase() // Init DB
        debugPrint(db)

        // Item 1: A downloaded file (should be in working set)
        var downloadedFile = remoteItemA.toItemMetadata(account: Self.account)
        downloadedFile.downloaded = true
        Self.dbManager.addItemMetadata(downloadedFile)

        // Item 2: A visited directory (should be in working set)
        var visitedDir = remoteFolder.toItemMetadata(account: Self.account)
        visitedDir.visitedDirectory = true
        Self.dbManager.addItemMetadata(visitedDir)

        // Item 3: A file that is in the DB but not downloaded (should NOT be in working set)
        var notDownloadedFile = remoteItemB.toItemMetadata(account: Self.account)
        notDownloadedFile.downloaded = false
        Self.dbManager.addItemMetadata(notDownloadedFile)

        // Item 4: A directory that is in the DB but not visited (should NOT be in working set)
        var notVisitedDir = remoteItemC.toItemMetadata(account: Self.account)
        notVisitedDir.directory = true
        notVisitedDir.visitedDirectory = false
        Self.dbManager.addItemMetadata(notVisitedDir)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)

        // 2. Act
        try await observer.enumerateItems()

        // 3. Assert
        XCTAssertNil(observer.error, "Enumeration should complete without error.")
        XCTAssertEqual(observer.items.count, 2, "Should only enumerate the 2 materialized items.")

        let enumeratedIds = Set(observer.items.map(\.itemIdentifier.rawValue))
        XCTAssertTrue(
            enumeratedIds.contains(downloadedFile.ocId),
            "The downloaded file should be in the working set."
        )
        XCTAssertTrue(
            enumeratedIds.contains(visitedDir.ocId),
            "The visited directory should be in the working set."
        )

        XCTAssertFalse(
            enumeratedIds.contains(notDownloadedFile.ocId),
            "The non-downloaded file should NOT be in the working set."
        )
        XCTAssertFalse(
            enumeratedIds.contains(notVisitedDir.ocId),
            "The non-visited directory should NOT be in the working set."
        )
    }

    func testWorkingSetEnumerationWhenNoMaterialisedItems() async throws {
        // This test verifies that the enumerator behaves correctly when there are
        // no materialized items in the database.

        // 1. Arrange
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        // Add items to the DB, but none are materialised.
        var notDownloadedFile = remoteItemA.toItemMetadata(account: Self.account)
        notDownloadedFile.downloaded = false
        Self.dbManager.addItemMetadata(notDownloadedFile)

        var notVisitedDir = remoteFolder.toItemMetadata(account: Self.account)
        notVisitedDir.visitedDirectory = false
        Self.dbManager.addItemMetadata(notVisitedDir)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)

        // 2. Act
        try await observer.enumerateItems()

        // 3. Assert
        XCTAssertNil(observer.error, "Enumeration should complete without error.")
        XCTAssertTrue(observer.items.isEmpty, "Result should be empty when no items materialised.")
    }

    func testReadServerUrlFollowUpPagination() async throws {
        // 1. Arrange: Setup a folder with enough children to require multiple pages.
        remoteFolder.children = []
        for i in 0 ..< 10 {
            let childItem = MockRemoteItem(
                identifier: "folderChild\(i)",
                name: "folderChild\(i).txt",
                remotePath: Self.account.davFilesUrl + "/folder/folderChild\(i).txt",
                account: Self.account.ncKitAccount,
                username: Self.account.username,
                userId: Self.account.id,
                serverUrl: Self.account.serverUrl
            )
            childItem.parent = remoteFolder
            remoteFolder.children.append(childItem)
        }

        let db = Self.dbManager.ncDatabase()
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, pagination: true)

        // Pre-populate the folder's metadata with an old etag to verify it gets updated
        // on the initial call.
        let oldEtag = "OLD_ETAG"
        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = oldEtag
        Self.dbManager.addItemMetadata(folderMetadata)

        // --- Scenario A: Initial Paginated Request (isFollowUpPaginatedRequest == false) ---

        // 2. Act: Call readServerUrl for the first page.
        let (initialMetadatas, _, _, _, initialNextPage, initialError) = await Enumerator.readServerUrl(
            remoteFolder.remotePath,
            pageSettings: (page: nil, index: 0, size: 5), // index is 0
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        // 3. Assert: Verify the initial request's behavior.
        XCTAssertNil(initialError)
        XCTAssertNotNil(initialNextPage, "Should receive a next page token for the initial request")

        // The first request for a folder returns the folder itself plus the first page of children.
        XCTAssertEqual(
            initialMetadatas?.count,
            4,
            """
            Should get target + first page of children,
            but the target should not be included in the first page,
            so count is (4).
            """
        )

        XCTAssertFalse(initialMetadatas?.contains(where: { $0.ocId == remoteFolder.identifier }) ?? false, "The folder itself should not be in the initial results.")

        // The logic inside `if !isFollowUpPaginatedRequest` should have run,
        // updating the folder's metadata.
        let updatedFolderMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotEqual(updatedFolderMetadata.etag, oldEtag, "The folder's etag should have been updated.")
        XCTAssertEqual(updatedFolderMetadata.etag, remoteFolder.versionIdentifier)

        // --- Scenario B: Follow-up Paginated Request (isFollowUpPaginatedRequest == true) ---

        // 4. Act: Call readServerUrl for the second page using the received page token.
        let followUpPage = try NSFileProviderPage(XCTUnwrap(initialNextPage?.token?.data(using: .utf8)))

        let (followUpMetadatas, _, _, _, finalNextPage, followUpError) = await Enumerator.readServerUrl(
            remoteFolder.remotePath,
            pageSettings: (page: followUpPage, index: 1, size: 5), // index > 0 and page is non-nil
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        // 5. Assert: Verify the follow-up request's behavior.
        XCTAssertNil(followUpError)
        XCTAssertNotNil(finalNextPage, "Should receive a next page token for the second request.")
        // A follow-up request should *only* contain the children for that page.
        XCTAssertEqual(
            followUpMetadatas?.count, 5, "Should get only the second page of children (5)."
        )
        XCTAssertFalse(
            followUpMetadatas?.contains(where: { $0.ocId == remoteFolder.identifier }) ?? true,
            "The folder itself should NOT be in the follow-up page results."
        )

        // This confirms the `if !isFollowUpPaginatedRequest` block was correctly skipped, as it'd
        // have processed the first item of the result array (`folderChild5`) as the parent folder,
        // which would lead to incorrect data or errors.
        let child5Metadata = Self.dbManager.itemMetadata(ocId: "folderChild5")
        XCTAssertNotNil(
            child5Metadata, "Metadata for the items on the second page should be in the database."
        )
    }

    func testHandlePagedReadResults() {
        // 1. Arrange
        let dbManager = Self.dbManager
        let db = dbManager.ncDatabase()
        debugPrint(db)

        let parentNKFile = remoteFolder.toNKFile()
        let childrenNKFiles = (0 ..< 5).map { i in
            MockRemoteItem(
                identifier: "pagedChild\(i)",
                name: "pagedChild\(i).txt",
                remotePath: Self.account.davFilesUrl + "/folder/pagedChild\(i).txt",
                account: Self.account.ncKitAccount,
                username: Self.account.username,
                userId: Self.account.id,
                serverUrl: Self.account.serverUrl
            ).toNKFile()
        }
        let followUpChildrenNKFiles = (5 ..< 10).map { i in
            MockRemoteItem(
                identifier: "pagedChild\(i)",
                name: "pagedChild\(i).txt",
                remotePath: Self.account.davFilesUrl + "/folder/pagedChild\(i).txt",
                account: Self.account.ncKitAccount,
                username: Self.account.username,
                userId: Self.account.id,
                serverUrl: Self.account.serverUrl
            ).toNKFile()
        }

        // --- Scenario A: First Page (pageIndex == 0) ---
        // 2. Act
        let firstPageFiles = [parentNKFile] + childrenNKFiles
        let (firstPageResult, firstPageError) = Enumerator.handlePagedReadResults(
            files: firstPageFiles, pageIndex: 0, dbManager: dbManager
        )

        // 3. Assert
        XCTAssertNil(firstPageError)
        // Result should only contain the children, not the parent.
        XCTAssertEqual(
            firstPageResult?.count, 5, "First page result should only contain children."
        )
        // The parent's metadata should be processed and saved to the DB.
        let parentMetadataFromDB = dbManager.itemMetadata(ocId: parentNKFile.ocId)
        XCTAssertNotNil(
            parentMetadataFromDB, "Parent folder metadata should be saved to DB from first page."
        )
        XCTAssertEqual(parentMetadataFromDB?.etag, parentNKFile.etag, "Parent etag should match.")
        // The children's metadata should also be saved.
        let childMetadataFromDB = dbManager.itemMetadata(ocId: "pagedChild0")
        XCTAssertNotNil(
            childMetadataFromDB, "Child metadata should be saved to DB from first page."
        )

        // --- Scenario B: Follow-up Page (pageIndex > 0) ---
        // 4. Act
        let (followUpPageResult, followUpPageError) = Enumerator.handlePagedReadResults(
            files: followUpChildrenNKFiles, pageIndex: 1, dbManager: dbManager
        )

        // 5. Assert
        XCTAssertNil(followUpPageError)
        // Result should contain all items passed in, as the parent is not included.
        XCTAssertEqual(
            followUpPageResult?.count, 5, "Follow-up page result should contain all its items."
        )
        let followUpChildMetadata = dbManager.itemMetadata(ocId: "pagedChild5")
        XCTAssertNotNil(
            followUpChildMetadata, "Child metadata should be saved to DB from follow-up page."
        )

        // --- Scenario C: Root Folder (should not be ingested) ---
        // 6. Act
        var rootNKFile = MockRemoteItem.rootItem(account: Self.account).toNKFile()
        // The check is based on URL string matching.
        rootNKFile.path = Self.account.davFilesUrl

        let (rootResult, rootError) = Enumerator.handlePagedReadResults(
            files: [rootNKFile], pageIndex: 0, dbManager: dbManager
        )

        // 7. Assert
        XCTAssertNil(rootError)
        // The result should be empty as there are no children.
        XCTAssertEqual(
            rootResult?.count, 0, "Root folder enumeration should yield no children."
        )
        // Metadata should be saved for the root folder ID itself.
        XCTAssertNotNil(
            dbManager.itemMetadata(ocId: rootNKFile.ocId),
            "Metadata for the root folder itself should be saved."
        )
    }

    func testWorkingSetEnumerateChanges() async throws {
        // This test verifies that `enumerateChanges` for the working set correctly
        // queries the local database for changes since the provided sync anchor date.

        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        let anchorDate = Date().addingTimeInterval(-300) // 5 minutes ago
        let tenMinutesAgo = Date().addingTimeInterval(-600)
        let now = Date()

        // Create a sync anchor from our date.
        let formatter = ISO8601DateFormatter()
        let anchor = try NSFileProviderSyncAnchor(XCTUnwrap(formatter.string(from: anchorDate).data(using: .utf8)))

        // Setup remote interface with the items we're testing
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // --- Database State ---
        var rootMetadata = rootItem.toItemMetadata(account: Self.account)
        rootMetadata.syncTime = now
        Self.dbManager.addItemMetadata(rootMetadata)

        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.syncTime = now
        Self.dbManager.addItemMetadata(folderMetadata)

        // Item synced BEFORE the anchor date (should not be reported).
        var oldItem = remoteItemA.toItemMetadata(account: Self.account)
        oldItem.downloaded = true // Materialised
        oldItem.syncTime = tenMinutesAgo
        Self.dbManager.addItemMetadata(oldItem)

        // Item synced AFTER the anchor date (should be reported as updated).
        var updatedItem = remoteItemB.toItemMetadata(account: Self.account)
        updatedItem.downloaded = true // Materialised
        updatedItem.deleted = false
        updatedItem.syncTime = now
        Self.dbManager.addItemMetadata(updatedItem)

        // Item marked as deleted AFTER the anchor date (should be reported as deleted).
        var deletedItem = remoteItemC.toItemMetadata(account: Self.account)
        deletedItem.downloaded = true // Materialised
        deletedItem.deleted = true
        deletedItem.syncTime = now
        Self.dbManager.addItemMetadata(deletedItem)

        // Non-materialised item synced after anchor date (should be ignored).
        var nonMaterialisedItem = MockRemoteItem(
            identifier: "non-mat",
            name: "non-mat.txt",
            remotePath: Self.account.davFilesUrl + "/non-mat.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        ).toItemMetadata(account: Self.account)
        nonMaterialisedItem.downloaded = false
        nonMaterialisedItem.syncTime = now
        Self.dbManager.addItemMetadata(nonMaterialisedItem)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)

        // 2. Act
        try await observer.enumerateChanges(from: anchor)

        // 3. Assert
        XCTAssertNil(observer.error, "Enumeration should complete without error.")

        // Check for updated items
        XCTAssertEqual(observer.changedItems.count, 2, "One item changed and the other did not but still its metadata was updated in the database.")

        let firstChangedItem = observer.changedItems[0]
        let secondChangedItem = observer.changedItems[1]

        XCTAssertEqual(firstChangedItem.itemIdentifier.rawValue, oldItem.ocId, "The item unchanged on the server should be reported as updated locally nonetheless.")
        XCTAssertEqual(secondChangedItem.itemIdentifier.rawValue, updatedItem.ocId, "The item which actually changed on the server should be reported as updated.")

        // Check for deleted items
        XCTAssertEqual(observer.deletedItemIdentifiers.count, 1, "There should be one deleted item.")
        XCTAssertEqual(observer.deletedItemIdentifiers.first?.rawValue, deletedItem.ocId, "The correct item should be reported as deleted.")
    }

    ///
    /// Avoid enumeration of child directories and their content when their parent directory's ETag has not changed.
    ///
    /// When checking the working set for changes, the file provider extension requests the latest state of every locally materialized directory from the server.
    /// Based on this the file provider extension can report changed items to the file provider framework.
    /// Because an unchanged directory ETag indicates the lack of changes in any of its descendants (no matter how far down the hierarchy) on the server, unnecessary requests for the server-side state of its descendants can be avoided.
    /// This test implements the verification of this expected optimization.
    ///
    func testChildDirectoriesOfAnUnchangedServerDirectoryShouldNotBeEnumerated() async throws {
        // 1. Setup: Create a directory hierarchy with a parent and child directory
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        // Create a child directory within remoteFolder
        let remoteChildFolder = MockRemoteItem(
            identifier: "childFolder",
            versionIdentifier: "CHILD_V1",
            name: "childFolder",
            remotePath: Self.account.davFilesUrl + "/folder/childFolder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        remoteChildFolder.parent = remoteFolder
        remoteFolder.children.append(remoteChildFolder)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Add parent folder to database with a specific ETag and mark as visited
        var parentFolderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        parentFolderMetadata.visitedDirectory = true
        parentFolderMetadata.etag = remoteFolder.versionIdentifier // Current ETag matches server
        Self.dbManager.addItemMetadata(parentFolderMetadata)

        // Add child folder to database and mark as visited (materialized)
        var childFolderMetadata = remoteChildFolder.toItemMetadata(account: Self.account)
        childFolderMetadata.visitedDirectory = true
        childFolderMetadata.etag = remoteChildFolder.versionIdentifier
        Self.dbManager.addItemMetadata(childFolderMetadata)

        // Create a sync anchor from before now
        let anchorDate = Date().addingTimeInterval(-300) // 5 minutes ago
        let formatter = ISO8601DateFormatter()
        let anchor = try NSFileProviderSyncAnchor(XCTUnwrap(formatter.string(from: anchorDate).data(using: .utf8)))

        // Update sync times to be after the anchor (so they would be checked)
        let now = Date()
        parentFolderMetadata.syncTime = now
        childFolderMetadata.syncTime = now
        Self.dbManager.addItemMetadata(parentFolderMetadata)
        Self.dbManager.addItemMetadata(childFolderMetadata)

        // 2. Act: Enumerate changes for the working set
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        let observer = MockChangeObserver(enumerator: enumerator)

        // Track how many times the remote interface is queried for each directory
        let initialReadCount = remoteInterface.readOperationCount

        try await observer.enumerateChanges(from: anchor)

        // 3. Assert: Verify optimization behavior
        XCTAssertNil(observer.error, "Enumeration should complete without error.")

        // Since the parent folder's ETag hasn't changed, the child folder should NOT
        // trigger a separate server request. The enumerator should recognize that
        // an unchanged parent ETag means no descendants have changed.

        // The parent folder should be checked (1 read), but the child should not
        // trigger an additional read since parent ETag is unchanged
        let finalReadCount = remoteInterface.readOperationCount
        let additionalReads = finalReadCount - initialReadCount

        // We expect reads for materialized directories, but child should be skipped
        // because parent ETag is unchanged
        XCTAssertLessThanOrEqual(
            additionalReads,
            1,
            "Child directory should not be enumerated when parent ETag is unchanged"
        )

        // Verify that no changes were reported for either folder since nothing changed
        XCTAssertEqual(
            observer.changedItems.count,
            4,
            "When checking the remote folder for changes on the server but there are none, still all 4 items in the local client database should be updated."
        )
        XCTAssertEqual(
            observer.deletedItemIdentifiers.count,
            0,
            "No items should be reported as deleted"
        )

        // Verify the metadata in the database remains correct
        let finalParentMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteFolder.identifier)
        )
        XCTAssertEqual(
            finalParentMetadata.etag,
            remoteFolder.versionIdentifier,
            "Parent folder ETag should remain unchanged"
        )

        let finalChildMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteChildFolder.identifier)
        )
        XCTAssertEqual(
            finalChildMetadata.etag,
            remoteChildFolder.versionIdentifier,
            "Child folder ETag should remain unchanged"
        )
    }

    func testFolderEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let oldEtag = "OLD"
        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = oldEtag

        Self.dbManager.addItemMetadata(folderMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 3)

        // A pass of enumerating a target should update the target too. Let's check.
        let dbFolderMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteFolder.identifier)
        )
        let storedFolderItemMaybe = await Item.storedItem(
            identifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let storedFolderItem = try XCTUnwrap(storedFolderItemMaybe)
        XCTAssertEqual(dbFolderMetadata.etag, remoteFolder.versionIdentifier)
        XCTAssertNotEqual(dbFolderMetadata.etag, oldEtag)
        XCTAssertEqual(storedFolderItem.childItemCount?.intValue, remoteFolder.children.count)
        XCTAssertEqual(storedFolderItem.isUploaded, true)

        let retrievedItemA = try XCTUnwrap(
            observer.items.first(where: { $0.itemIdentifier.rawValue == remoteItemA.identifier })
        )
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(retrievedItemA.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )
        XCTAssertEqual(retrievedItemA.isDownloaded, false)
        XCTAssertEqual(retrievedItemA.isUploaded, true)
    }

    func testEnumerateFile() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        var itemAMetadata = remoteItemA.toItemMetadata(account: Self.account)
        itemAMetadata.downloaded = true

        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemAMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemA.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 1)

        let retrievedItemAItem = try XCTUnwrap(observer.items.first)
        XCTAssertEqual(retrievedItemAItem.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemAItem.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemAItem.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemAItem.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(retrievedItemAItem.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )
        XCTAssertEqual(retrievedItemAItem.isDownloaded, true)
        XCTAssertEqual(retrievedItemAItem.isUploaded, true)

        let dbItemAMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemA.identifier)
        )
        XCTAssertEqual(dbItemAMetadata.ocId, remoteItemA.identifier)
        XCTAssertEqual(dbItemAMetadata.etag, remoteItemA.versionIdentifier)
        XCTAssertTrue(dbItemAMetadata.downloaded)

        // Check download state is not just always true
        Self.dbManager.addItemMetadata(remoteItemB.toItemMetadata(account: Self.account))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemB.identifier))
        let enumerator2 = Enumerator(
            enumeratedItemIdentifier: .init(remoteItemB.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer2 = MockEnumerationObserver(enumerator: enumerator2)
        try await observer2.enumerateItems()
        XCTAssertEqual(observer2.items.count, 1)

        let dbItemBMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemB.identifier)
        )
        XCTAssertEqual(dbItemBMetadata.ocId, remoteItemB.identifier)
        XCTAssertEqual(dbItemBMetadata.etag, remoteItemB.versionIdentifier)
        XCTAssertFalse(dbItemBMetadata.downloaded)
    }

    func testFolderAndContentsChangeEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        remoteFolder.children.removeAll(where: { $0.identifier == remoteItemB.identifier })
        remoteFolder.children.append(remoteItemC)
        remoteItemC.parent = remoteFolder

        let oldFolderEtag = "OLD"
        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = oldFolderEtag
        folderMetadata.downloaded = true // Test downloaded state is properly retained

        let oldItemAEtag = "OLD"
        var itemAMetadata = remoteItemA.toItemMetadata(account: Self.account)
        itemAMetadata.etag = oldItemAEtag
        itemAMetadata.downloaded = true // Test downloaded state is properly retained

        let itemBMetadata = remoteItemB.toItemMetadata(account: Self.account)

        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemAMetadata)
        Self.dbManager.addItemMetadata(itemBMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemA.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemB.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        // There are four changes: changed folder, changed Item A, removed Item B, added Item C
        XCTAssertEqual(observer.changedItems.count, 3)
        XCTAssertTrue(observer.changedItems.contains(
            where: { $0.itemIdentifier.rawValue == remoteItemA.identifier }
        ))
        XCTAssertTrue(observer.changedItems.contains(
            where: { $0.itemIdentifier.rawValue == remoteItemC.identifier }
        ))
        XCTAssertEqual(observer.deletedItemIdentifiers.count, 1)
        XCTAssertTrue(observer.deletedItemIdentifiers.contains(
            where: { $0.rawValue == remoteItemB.identifier }
        ))

        // A pass of enumerating a target should update the target too. Let's check.
        let dbFolderMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteFolder.identifier)
        )
        let dbItemAMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemA.identifier)
        )
        let dbItemBMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemB.identifier)
        )
        let dbItemCMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemC.identifier)
        )
        XCTAssertTrue(dbItemBMetadata.deleted)
        XCTAssertEqual(dbFolderMetadata.etag, remoteFolder.versionIdentifier)
        XCTAssertTrue(dbFolderMetadata.downloaded)
        XCTAssertEqual(dbItemAMetadata.etag, remoteItemA.versionIdentifier)
        XCTAssertNotEqual(dbItemAMetadata.etag, oldItemAEtag)
        XCTAssertTrue(dbItemAMetadata.downloaded)
        XCTAssertEqual(dbItemCMetadata.ocId, remoteItemC.identifier)
        XCTAssertEqual(dbItemCMetadata.etag, remoteItemC.versionIdentifier)
        XCTAssertEqual(dbItemCMetadata.fileName, remoteItemC.name)
        XCTAssertEqual(dbItemCMetadata.fileNameView, remoteItemC.name)
        XCTAssertEqual(dbItemCMetadata.serverUrl, remoteFolder.remotePath)
        XCTAssertEqual(dbItemCMetadata.account, Self.account.ncKitAccount)
        XCTAssertEqual(dbItemCMetadata.user, Self.account.username)
        XCTAssertEqual(dbItemCMetadata.userId, Self.account.id)
        XCTAssertEqual(dbItemCMetadata.urlBase, Self.account.serverUrl)
        XCTAssertFalse(dbItemCMetadata.downloaded)

        let storedFolderItemMaybe = await Item.storedItem(
            identifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        XCTAssertNotNil(storedFolderItemMaybe)

        let retrievedItemA = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemA.identifier }
        ))
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(retrievedItemA.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )

        let retrievedItemC = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemC.identifier }
        ))
        XCTAssertEqual(retrievedItemC.itemIdentifier.rawValue, remoteItemC.identifier)
        XCTAssertEqual(retrievedItemC.filename, remoteItemC.name)
        XCTAssertEqual(retrievedItemC.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemC.creationDate, remoteItemC.creationDate)
        XCTAssertEqual(
            Int(retrievedItemC.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemC.modificationDate.timeIntervalSince1970)
        )
    }

    func testFileMoveChangeEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        remoteFolder.children.removeAll(where: { $0.identifier == remoteItemA.identifier })
        rootItem.children.append(remoteItemA)
        remoteItemA.parent = rootItem
        remoteItemA.remotePath = rootItem.remotePath + "/\(remoteItemA.name)"

        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = "OLD"

        let oldEtag = "OLD"
        let oldServerUrl = remoteFolder.remotePath
        let oldName = "oldItemA"
        var itemAMetadata = remoteItemA.toItemMetadata(account: Self.account)
        itemAMetadata.etag = oldEtag
        itemAMetadata.name = oldName
        itemAMetadata.fileName = oldName
        itemAMetadata.fileNameView = oldName
        itemAMetadata.serverUrl = oldServerUrl

        let itemBMetadata = remoteItemB.toItemMetadata(account: Self.account)

        Self.dbManager.addItemMetadata(folderMetadata)
        Self.dbManager.addItemMetadata(itemAMetadata)
        Self.dbManager.addItemMetadata(itemBMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemA.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemB.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        // rootContainer has changed, folder has changed, itemA has changed
        XCTAssertEqual(observer.changedItems.count, 3)
        XCTAssertTrue(observer.deletedItemIdentifiers.isEmpty)

        let retrievedItemA = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemA.identifier }
        ))
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(retrievedItemA.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )

        let storedItemAMaybe = await Item.storedItem(
            identifier: .init(remoteItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let storedItemA = try XCTUnwrap(storedItemAMaybe)
        XCTAssertEqual(storedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(storedItemA.filename, remoteItemA.name)
        XCTAssertEqual(storedItemA.parentItemIdentifier.rawValue, rootItem.identifier)
        XCTAssertEqual(storedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(storedItemA.contentModificationDate?.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )

        let storedRootItem = await Item.rootContainer(
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            remoteSupportsTrash: remoteInterface.supportsTrash(account: Self.account),
            log: FileProviderLogMock()
        )
        print(storedRootItem.metadata.serverUrl)
        XCTAssertEqual(storedRootItem.childItemCount?.intValue, 4) // All items

        let storedFolderMaybe = await Item.storedItem(
            identifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let storedFolder = try XCTUnwrap(storedFolderMaybe)
        XCTAssertEqual(storedFolder.childItemCount?.intValue, remoteFolder.children.count)
    }

    func testFileLockStateEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        remoteFolder.children.append(remoteItemC)
        remoteItemC.parent = remoteFolder

        remoteItemA.locked = true
        remoteItemA.lockOwner = Self.account.username
        remoteItemA.lockTimeOut = Date.now.advanced(by: 1_000_000_000_000)

        remoteItemB.locked = true
        remoteItemB.lockOwner = "other different account"
        remoteItemB.lockTimeOut = Date.now.advanced(by: 1_000_000_000_000)

        remoteItemC.locked = true
        remoteItemC.lockOwner = "other different account"
        remoteItemC.lockTimeOut = Date.now.advanced(by: -1_000_000_000_000)

        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = "OLD"

        Self.dbManager.addItemMetadata(folderMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 4)

        let dbItemAMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemA.identifier)
        )
        let dbItemBMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemB.identifier)
        )
        let dbItemCMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemC.identifier)
        )

        XCTAssertEqual(dbItemAMetadata.lock, remoteItemA.locked)
        XCTAssertEqual(dbItemAMetadata.lockOwner, remoteItemA.lockOwner)
        XCTAssertEqual(dbItemAMetadata.lockTimeOut, remoteItemA.lockTimeOut)

        XCTAssertEqual(dbItemBMetadata.lock, remoteItemB.locked)
        XCTAssertEqual(dbItemBMetadata.lockOwner, remoteItemB.lockOwner)
        XCTAssertEqual(dbItemBMetadata.lockTimeOut, remoteItemB.lockTimeOut)

        XCTAssertEqual(dbItemCMetadata.lock, remoteItemC.locked)
        XCTAssertEqual(dbItemCMetadata.lockOwner, remoteItemC.lockOwner)
        XCTAssertEqual(dbItemCMetadata.lockTimeOut, remoteItemC.lockTimeOut)

        let storedItemAMaybe = await Item.storedItem(
            identifier: .init(remoteItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let storedItemA = try XCTUnwrap(storedItemAMaybe)
        let storedItemBMaybe = await Item.storedItem(
            identifier: .init(remoteItemB.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let storedItemB = try XCTUnwrap(storedItemBMaybe)
        let storedItemCMaybe = await Item.storedItem(
            identifier: .init(remoteItemC.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let storedItemC = try XCTUnwrap(storedItemCMaybe)

        // Should be able to write to files locked by self
        XCTAssertTrue(storedItemA.fileSystemFlags.contains(.userWritable))
        // Should not be able to write to files locked by someone else
        XCTAssertFalse(storedItemB.fileSystemFlags.contains(.userWritable))
        // Should be able to write to files with an expired lock
        XCTAssertTrue(storedItemC.fileSystemFlags.contains(.userWritable))
    }

    /// File Provider system will panic if we give it an NSFileProviderItem with an empty filename.
    /// Test that we have a fallback to avoid this, even if something catastrophic happens in the
    /// server and the file has no filename
    func testEnsureNoEmptyItemNameEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        remoteItemA.name = ""
        remoteItemA.parent = remoteInterface.rootItem
        rootItem.children = [remoteItemA]

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        // rootContainer has changed, itemA has changed
        XCTAssertEqual(observer.changedItems.count, 2)

        let dbItemAMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemA.identifier)
        )
        XCTAssertEqual(dbItemAMetadata.ocId, remoteItemA.identifier)
        XCTAssertEqual(dbItemAMetadata.fileName, remoteItemA.name)

        let storedItemAMaybe = await Item.storedItem(
            identifier: .init(remoteItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let storedItemA = try XCTUnwrap(storedItemAMaybe)
        XCTAssertEqual(storedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertNotEqual(storedItemA.filename, remoteItemA.name)
        XCTAssertFalse(storedItemA.filename.isEmpty)
    }

    func testTrashEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
    }

    func testTrashChangeEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        rootTrashItem.children = [remoteTrashItemA]
        remoteTrashItemA.parent = rootTrashItem
        remoteTrashItemB.parent = nil
        remoteTrashItemC.parent = nil

        Self.dbManager.addItemMetadata(
            remoteTrashItemA.toNKTrash().toItemMetadata(account: Self.account)
        )
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemA.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 0)
        observer.reset()

        rootTrashItem.children = [remoteTrashItemA, remoteTrashItemB]
        remoteTrashItemB.parent = rootTrashItem
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 0)
        observer.reset()

        rootTrashItem.children = [remoteTrashItemB, remoteTrashItemC]
        remoteTrashItemA.parent = nil
        remoteTrashItemC.parent = rootTrashItem
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 0)
        XCTAssertEqual(observer.deletedItemIdentifiers.count, 1)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: remoteTrashItemA.identifier)?.deleted, true)
    }

    func testTrashItemEnumerationFailWhenNoTrashInCapabilities() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""undelete": true,"##, with: "")

        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        do {
            try await observer.enumerateItems()
            XCTFail("Item enumeration should have failed!")
        } catch {
            XCTAssertEqual((error as NSError?)?.code, NSFeatureUnsupportedError)
        }
    }

    func testKeepDownloadedRetainedDuringEnumeration() async throws {
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let existingFolder = remoteFolder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(existingFolder)

        // Setup existing item with keepDownloaded = true
        var existingItem = remoteItemA.toItemMetadata(account: Self.account)
        existingItem.keepDownloaded = true
        existingItem.downloaded = true
        Self.dbManager.addItemMetadata(existingItem)

        // Simulate server response with updated etag but no keepDownloaded
        remoteFolder.versionIdentifier = "NEW"
        remoteItemA.versionIdentifier = "NEW_ETAG"

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()

        // Verify the updated metadata
        let updatedMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemA.identifier)
        )
        XCTAssertTrue(updatedMetadata.keepDownloaded, "keepDownloaded should remain true after enumeration")
        XCTAssertEqual(updatedMetadata.etag, "NEW_ETAG", "Etag should be updated")
        XCTAssertTrue(updatedMetadata.downloaded, "Downloaded state should be preserved")
    }

    func testTrashChangeEnumerationFailWhenNoTrashInCapabilities() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""undelete": true,"##, with: "")

        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        do {
            try await observer.enumerateChanges()
            XCTFail("Item enumeration should have failed!")
        } catch {
            XCTAssertEqual((error as NSError?)?.code, NSFeatureUnsupportedError)
        }
    }

    func testRemoteLockFilesNotEnumerated() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, rootTrashItem: rootTrashItem)

        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem

        let remoteLockFileItem = MockRemoteItem(
            identifier: "lock-file",
            name: "~$lock-file.docx",
            remotePath: Self.account.davFilesUrl + "/" + remoteFolder.name + "/~$lock-file.docx",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children.append(remoteLockFileItem)
        remoteLockFileItem.parent = rootItem

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 2)
        XCTAssertFalse(
            observer.items.contains(where: { $0.itemIdentifier.rawValue == "lock-file" })
        )
    }

    /// Tests situation where we are enumerating files and we can no longer find the parent item
    /// in the database. So we need to simulate a situation where this takes place.
    func testCorrectEnumerateFileWithMissingParentInDb() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        var itemAMetadata = remoteItemA.toItemMetadata(account: Self.account)
        itemAMetadata.etag = "OLD"

        Self.dbManager.addItemMetadata(itemAMetadata)
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemA.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 2) // Must include the folder that was missing
        XCTAssertTrue(observer.deletedItemIdentifiers.isEmpty)

        let retrievedItemA = try XCTUnwrap(observer.changedItems.first(
            where: { $0.itemIdentifier.rawValue == remoteItemA.identifier }
        ))
        XCTAssertEqual(retrievedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(retrievedItemA.filename, remoteItemA.name)
        XCTAssertEqual(retrievedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(retrievedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(retrievedItemA.contentModificationDate??.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )

        let storedItemAMaybe = await Item.storedItem(
            identifier: .init(remoteItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let storedItemA = try XCTUnwrap(storedItemAMaybe)
        XCTAssertEqual(storedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertEqual(storedItemA.filename, remoteItemA.name)
        XCTAssertEqual(storedItemA.parentItemIdentifier.rawValue, remoteFolder.identifier)
        XCTAssertEqual(storedItemA.creationDate, remoteItemA.creationDate)
        XCTAssertEqual(
            Int(storedItemA.contentModificationDate?.timeIntervalSince1970 ?? 0),
            Int(remoteItemA.modificationDate.timeIntervalSince1970)
        )
    }

    func testFolderPaginatedEnumeration() async throws {
        remoteFolder.children = []
        for i in 0 ... 20 {
            let childItem = MockRemoteItem(
                identifier: "folderChild\(i)",
                name: "folderChild\(i).txt",
                remotePath: Self.account.davFilesUrl + "folder/folderChild\(i).txt",
                directory: i % 5 == 0,
                account: Self.account.ncKitAccount,
                username: Self.account.username,
                userId: Self.account.id,
                serverUrl: Self.account.serverUrl
            )
            childItem.parent = remoteFolder
            remoteFolder.children.append(childItem)
        }

        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, pagination: true)

        let oldEtag = "OLD"
        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = oldEtag

        Self.dbManager.addItemMetadata(folderMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            pageSize: 5,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 22)

        for item in observer.items {
            XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue))
        }
        XCTAssertEqual(
            observer.items.count(where: { $0.contentType?.conforms(to: .folder) ?? false }),
            6
        )
        XCTAssertTrue(observer.items.last?.contentType?.conforms(to: .folder) ?? false)

        XCTAssertEqual(observer.observedPages.first, NSFileProviderPage.initialPageSortedByName as NSFileProviderPage)
        // XCTAssertEqual(observer.observedPages.count, 5)
    }

    func testEmptyFolderPaginatedEnumeration() async throws {
        // 1. Setup: remoteFolder exists in the DB but has no children.
        // Ensure the folder itself is in the database, as the enumerator for a specific item
        // will try to fetch its metadata.
        remoteFolder.children = [] // Ensure it's empty for this test
        Self.dbManager.addItemMetadata(remoteFolder.toItemMetadata(account: Self.account))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier), "Folder metadata should be in DB for enumeration.")

        let db = Self.dbManager.ncDatabase() // Strong ref for in-memory test db
        debugPrint(db)
        // Enable pagination in MockRemoteInterface to ensure the pagination path is taken
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, pagination: true)

        // 2. Create enumerator for the empty folder with a specific pageSize.
        let enumerator = Enumerator(
            enumeratedItemIdentifier: NSFileProviderItemIdentifier(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            pageSize: 5, // Page size can be anything, as the folder is empty
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)

        // 3. Enumerate items.
        try await observer.enumerateItems()

        // 4. Assertions.
        // When enumerating a folder (even an empty one) with depth .targetAndDirectChildren,
        // the folder item itself should be returned.
        XCTAssertEqual(observer.items.count, 1, "Should enumerate nothing.")

        // For an empty folder, there's only one "page" of results (the folder itself).
        XCTAssertEqual(observer.observedPages.count, 1, "Should be one page call for an empty folder.")

        // Verify the folder's metadata in the database is up-to-date.
        // This ensures the enumeration process also updates the target item's metadata if necessary
        let dbFolderMetadata =
            try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertEqual(
            dbFolderMetadata.etag,
            remoteFolder.versionIdentifier,
            "Folder ETag should be updated in DB if changed by enumeration."
        )
        let storedFolderItem = Item(
            metadata: dbFolderMetadata,
            parentItemIdentifier: .init(rootItem.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let childItemCount = storedFolderItem.childItemCount as? Int
        let expectedChildItemCount = remoteFolder.children.count
        XCTAssertEqual(childItemCount, expectedChildItemCount)
    }

    func testFolderWithFewItemsPaginatedEnumeration() async throws {
        // 1. Setup: remoteFolder with 3 children (fewer than pageSize 5).
        // Add folder metadata to DB.
        remoteFolder.children = []
        for i in 0 ..< 3 {
            let childItem = MockRemoteItem(
                identifier: "fewItems-child\(i)",
                name: "child\(i).txt",
                remotePath: remoteFolder.remotePath + "/child\(i).txt",
                account: Self.account.ncKitAccount,
                username: Self.account.username,
                userId: Self.account.id,
                serverUrl: Self.account.serverUrl
            )
            childItem.parent = remoteFolder
            remoteFolder.children.append(childItem)
        }
        Self.dbManager.addItemMetadata(remoteFolder.toItemMetadata(account: Self.account))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))

        let db = Self.dbManager.ncDatabase()
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem, pagination: true)

        // 2. Create enumerator with pageSize > number of children.
        let enumerator = Enumerator(
            enumeratedItemIdentifier: NSFileProviderItemIdentifier(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            pageSize: 5,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)

        // 3. Enumerate items.
        try await observer.enumerateItems()

        // 4. Assertions.
        // Expected items: 1 (folder itself) + 3 children = 3 items.
        XCTAssertEqual(observer.items.count, 4, "Should enumerate the folder and 3 children.")
        XCTAssertTrue(
            observer.items.contains(where: { $0.itemIdentifier.rawValue == remoteFolder.identifier }),
            "Folder itself should be enumerated."
        )
        for i in 0 ..< 3 {
            XCTAssertTrue(
                observer.items.contains(where: { $0.itemIdentifier.rawValue == "fewItems-child\(i)" }),
                "Child item fewItems-child\(i) should be enumerated."
            )
        }

        // All items fit on one page.
        XCTAssertEqual(
            observer.observedPages.count,
            1,
            "Should be one page call as all items fit in the first page."
        )

        // Verify folder metadata in DB.
        let dbFolderMetadata =
            try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertEqual(dbFolderMetadata.etag, remoteFolder.versionIdentifier)
        // Ensure all children are also in the DB after enumeration
        for i in 0 ..< 3 {
            XCTAssertNotNil(
                Self.dbManager.itemMetadata(ocId: "fewItems-child\(i)"),
                "Child item fewItems-child\(i) metadata should be in DB."
            )
        }
    }

    func testVisitedDirectoryStatePreservedDuringUpdate() async throws {
        // This test verifies that visitedDirectory state is preserved when updating
        // existing folder metadata during enumeration, addressing the fix in FilesDatabaseManager
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Setup root container metadata in database (required for enumeration)
        Self.dbManager.addItemMetadata(rootItem.toItemMetadata(account: Self.account))

        // 1. Setup: Create a folder that has already been visited (visitedDirectory = true)
        var existingFolderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        existingFolderMetadata.visitedDirectory = true
        existingFolderMetadata.etag = "OLD_ETAG"
        Self.dbManager.addItemMetadata(existingFolderMetadata)

        // Verify initial state
        let initialMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertTrue(initialMetadata.visitedDirectory, "Folder should initially be marked as visited")
        XCTAssertEqual(initialMetadata.etag, "OLD_ETAG")

        // 2. Act: Enumerate the folder, which will trigger an update
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()

        // 3. Assert: Verify that visitedDirectory state is preserved after update
        let updatedMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertTrue(updatedMetadata.visitedDirectory,
                      "visitedDirectory state should be preserved during update")
        XCTAssertEqual(updatedMetadata.etag, remoteFolder.versionIdentifier,
                       "ETag should be updated to new value")
        XCTAssertNotEqual(updatedMetadata.etag, "OLD_ETAG",
                          "ETag should have changed from old value")
    }

    func testVisitedDirectorySetDuringDirectoryRead() async throws {
        // This test verifies that visitedDirectory is correctly set to true
        // when a directory is the target of a depth-1 read operation
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Setup root container metadata in database (required for enumeration)
        Self.dbManager.addItemMetadata(rootItem.toItemMetadata(account: Self.account))

        // 1. Setup: Create a new folder that hasn't been visited yet
        var newFolderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        newFolderMetadata.visitedDirectory = false
        newFolderMetadata.etag = "INITIAL_ETAG"
        Self.dbManager.addItemMetadata(newFolderMetadata)

        // Verify initial state
        let initialMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertFalse(initialMetadata.visitedDirectory, "Folder should initially not be marked as visited")

        // 2. Act: Enumerate the folder (depth-1 read)
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()

        // 3. Assert: Verify that visitedDirectory is now set to true
        let updatedMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertTrue(updatedMetadata.visitedDirectory,
                      "visitedDirectory should be set to true after directory enumeration")
        XCTAssertEqual(updatedMetadata.etag, remoteFolder.versionIdentifier,
                       "ETag should be updated")
    }

    func testVisitedDirectoryStateInWorkingSet() async throws {
        // This test verifies that folders marked as visitedDirectory appear in working set
        // and that the state is preserved correctly across operations
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Setup root container metadata in database (required for enumeration)
        Self.dbManager.addItemMetadata(rootItem.toItemMetadata(account: Self.account))

        // 1. Setup: Create folders with different visited states
        var visitedFolder = remoteFolder.toItemMetadata(account: Self.account)
        visitedFolder.visitedDirectory = true
        visitedFolder.downloaded = false // Not downloaded, but visited
        Self.dbManager.addItemMetadata(visitedFolder)

        var notVisitedFolder = MockRemoteItem(
            identifier: "notVisitedFolder",
            versionIdentifier: "V1",
            name: "NotVisitedFolder",
            remotePath: Self.account.davFilesUrl + "/NotVisitedFolder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        ).toItemMetadata(account: Self.account)
        notVisitedFolder.visitedDirectory = false
        notVisitedFolder.downloaded = false
        Self.dbManager.addItemMetadata(notVisitedFolder)

        // 2. Act: Enumerate working set
        let workingSetEnumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let workingSetObserver = MockEnumerationObserver(enumerator: workingSetEnumerator)
        try await workingSetObserver.enumerateItems()

        // 3. Assert: Only visited folder should be in working set
        let workingSetIds = Set(workingSetObserver.items.map(\.itemIdentifier.rawValue))
        XCTAssertTrue(workingSetIds.contains(visitedFolder.ocId),
                      "Visited folder should be in working set")
        XCTAssertFalse(workingSetIds.contains(notVisitedFolder.ocId),
                       "Non-visited folder should not be in working set")

        // 4. Act: Now enumerate the not-visited folder to make it visited
        // Add the not-visited folder to the remote structure for enumeration
        let notVisitedRemoteFolder = MockRemoteItem(
            identifier: notVisitedFolder.ocId,
            versionIdentifier: "V2",
            name: notVisitedFolder.fileName,
            remotePath: notVisitedFolder.serverUrl + "/" + notVisitedFolder.fileName,
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children.append(notVisitedRemoteFolder)
        notVisitedRemoteFolder.parent = rootItem

        let folderEnumerator = Enumerator(
            enumeratedItemIdentifier: .init(notVisitedFolder.ocId),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let folderObserver = MockEnumerationObserver(enumerator: folderEnumerator)
        try await folderObserver.enumerateItems()

        // 5. Assert: Verify the folder is now marked as visited
        let updatedNotVisitedMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: notVisitedFolder.ocId))
        XCTAssertTrue(updatedNotVisitedMetadata.visitedDirectory,
                      "Folder should now be marked as visited after enumeration")

        // 6. Act: Enumerate working set again
        let workingSetEnumerator2 = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let workingSetObserver2 = MockEnumerationObserver(enumerator: workingSetEnumerator2)
        try await workingSetObserver2.enumerateItems()

        // 7. Assert: Both folders should now be in working set
        let workingSetIds2 = Set(workingSetObserver2.items.map(\.itemIdentifier.rawValue))
        XCTAssertTrue(workingSetIds2.contains(visitedFolder.ocId),
                      "Original visited folder should still be in working set")
        XCTAssertTrue(workingSetIds2.contains(notVisitedFolder.ocId),
                      "Newly visited folder should now be in working set")
    }

    // MARK: - Pagination Tests (Server Version Detection)

    /// Test that pagination is enabled when server is Nextcloud 31 or newer
    func testPaginationEnabledForNC31Plus() async throws {
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        // Setup a folder with many children to trigger pagination
        remoteFolder.children = []
        for i in 0 ..< 25 {
            let childItem = MockRemoteItem(
                identifier: "paginatedChild\(i)",
                name: "file_\(i).pdf",
                remotePath: Self.account.davFilesUrl + "/folder/file_\(i).pdf",
                data: Data(repeating: UInt8(i % 256), count: 100),
                account: Self.account.ncKitAccount,
                username: Self.account.username,
                userId: Self.account.id,
                serverUrl: Self.account.serverUrl
            )
            childItem.parent = remoteFolder
            remoteFolder.children.append(childItem)
        }

        // Create remote interface with pagination enabled and NC31+ capabilities
        let remoteInterface = MockRemoteInterface(
            account: Self.account, rootItem: rootItem, pagination: true
        )
        // Override capabilities to simulate NC31+
        remoteInterface.capabilities = ##"""
        {
          "ocs": {
            "data": {
              "version": {
                "major": 31,
                "minor": 0,
                "micro": 0,
                "string": "31.0.0"
              }
            }
          }
        }
        """##

        Self.dbManager.addItemMetadata(remoteFolder.toItemMetadata(account: Self.account))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            pageSize: 5, // Small page size to force multiple pages
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()

        // With pagination enabled, all items should be enumerated
        XCTAssertEqual(
            observer.items.count,
            26, // folder + 25 children
            "Pagination should enumerate all items for NC31+"
        )

        // Verify all items are in database
        for i in 0 ..< 25 {
            XCTAssertNotNil(
                Self.dbManager.itemMetadata(ocId: "paginatedChild\(i)"),
                "Child item paginatedChild\(i) should be in DB with pagination enabled"
            )
        }
    }

    /// Test that pagination is disabled when server is older than Nextcloud 31
    func testPaginationDisabledForOldServers() async throws {
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        // Setup a folder with children
        remoteFolder.children = []
        for i in 0 ..< 10 {
            let childItem = MockRemoteItem(
                identifier: "oldServerChild\(i)",
                name: "file_\(i).txt",
                remotePath: Self.account.davFilesUrl + "/folder/file_\(i).txt",
                data: Data(repeating: UInt8(i % 256), count: 50),
                account: Self.account.ncKitAccount,
                username: Self.account.username,
                userId: Self.account.id,
                serverUrl: Self.account.serverUrl
            )
            childItem.parent = remoteFolder
            remoteFolder.children.append(childItem)
        }

        // Create remote interface with pagination NOT enabled (simulates old server)
        let remoteInterface = MockRemoteInterface(
            account: Self.account, rootItem: rootItem, pagination: false
        )
        // Override capabilities to simulate NC30
        remoteInterface.capabilities = ##"""
        {
          "ocs": {
            "data": {
              "version": {
                "major": 30,
                "minor": 0,
                "micro": 5,
                "string": "30.0.5"
              }
            }
          }
        }
        """##

        Self.dbManager.addItemMetadata(remoteFolder.toItemMetadata(account: Self.account))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            pageSize: 5, // Page size doesn't matter when pagination is disabled
            log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()

        // With pagination disabled, all items should still be enumerated (in single request)
        XCTAssertEqual(
            observer.items.count,
            11, // folder + 10 children
            "Non-paginated enumeration should work for older servers"
        )

        // Verify all items are in database
        for i in 0 ..< 10 {
            XCTAssertNotNil(
                Self.dbManager.itemMetadata(ocId: "oldServerChild\(i)"),
                "Child item oldServerChild\(i) should be in DB even without pagination"
            )
        }
    }
}
