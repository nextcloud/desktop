//
//  EnumeratorTests.swift
//
//
//  Created by Claudio Cambra on 14/5/24.
//

import FileProvider
import NextcloudKit
import RealmSwift
import XCTest
@testable import TestInterface
@testable import NextcloudFileProviderKit

final class EnumeratorTests: XCTestCase {
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

    static let dbManager = FilesDatabaseManager(
        realmConfig: .defaultConfiguration, account: account
    )

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
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 1)

        let retrievedFolderItem = try XCTUnwrap(observer.items.first)
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
            dbManager: Self.dbManager
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
            remoteInterface: MockRemoteInterface(),
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)

        // 2. Act
        try await observer.enumerateItems()

        // 3. Assert
        XCTAssertNil(observer.error, "Enumeration should complete without error.")
        XCTAssertEqual(observer.items.count, 2, "Should only enumerate the 2 materialised items.")

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
        // no materialised items in the database.

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
            remoteInterface: MockRemoteInterface(),
            dbManager: Self.dbManager
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
        for i in 0..<10 {
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
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, pagination: true)

        // Pre-populate the folder's metadata with an old etag to verify it gets updated
        // on the initial call.
        let oldEtag = "OLD_ETAG"
        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = oldEtag
        Self.dbManager.addItemMetadata(folderMetadata)

        // --- Scenario A: Initial Paginated Request (isFollowUpPaginatedRequest == false) ---

        // 2. Act: Call readServerUrl for the first page.
        let (
            initialMetadatas, _, _, _, initialNextPage, initialError
        ) = await Enumerator.readServerUrl(
            remoteFolder.remotePath,
            pageSettings: (page: nil, index: 0, size: 5), // index is 0
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
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
        XCTAssertFalse(
            initialMetadatas?.contains(where: { $0.ocId == remoteFolder.identifier }) ?? false,
            "The folder itself should not be in the initial results."
        )

        // The logic inside `if !isFollowUpPaginatedRequest` should have run,
        // updating the folder's metadata.
        let updatedFolderMetadata =
            try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotEqual(
            updatedFolderMetadata.etag, oldEtag, "The folder's etag should have been updated."
        )
        XCTAssertEqual(updatedFolderMetadata.etag, remoteFolder.versionIdentifier)

        // --- Scenario B: Follow-up Paginated Request (isFollowUpPaginatedRequest == true) ---

        // 4. Act: Call readServerUrl for the second page using the received page token.
        let followUpPage = NSFileProviderPage(initialNextPage!.token!.data(using: .utf8)!)
        let (
            followUpMetadatas, _, _, _, finalNextPage, followUpError
        ) = await Enumerator.readServerUrl(
            remoteFolder.remotePath,
            pageSettings: (page: followUpPage, index: 1, size: 5), // index > 0 and page is non-nil
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
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

    func testHandlePagedReadResults() throws {
        // 1. Arrange
        let dbManager = Self.dbManager
        let db = dbManager.ncDatabase()
        debugPrint(db)

        let parentNKFile = remoteFolder.toNKFile()
        let childrenNKFiles = (0..<5).map { i in
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
        let followUpChildrenNKFiles = (5..<10).map { i in
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
        // This test verifies that `enumerateChanges` for the working set correctly identifies
        // new, updated, and deleted items by iterating through materialised items and fetching
        // their state.

        // 1. Arrange
        let db = Self.dbManager.ncDatabase()
        debugPrint(db)

        // --- Database State (The "local truth" before enumeration) ---
        var root = rootItem.toItemMetadata(account: Self.account)
        root.visitedDirectory = true
        root.etag = "ETAG_OLD"
        Self.dbManager.addItemMetadata(root)

        // A materialised folder that we will check for changes.
        var materialisedFolder = remoteFolder.toItemMetadata(account: Self.account)
        materialisedFolder.visitedDirectory = true
        materialisedFolder.etag = "ETAG_OLD"
        Self.dbManager.addItemMetadata(materialisedFolder)

        let remoteUnmaterialisedFolder = MockRemoteItem(
            identifier: "unmaterialised-folder",
            versionIdentifier: "NEW",
            name: "unmaterialised folder",
            remotePath: Self.account.davFilesUrl + "/" + "unmaterialised folder",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteUnmaterialisedFolder.parent = rootItem
        rootItem.children.append(remoteUnmaterialisedFolder)

        var unmaterialisedFolder = remoteUnmaterialisedFolder.toItemMetadata(account: Self.account)
        unmaterialisedFolder.visitedDirectory = false
        unmaterialisedFolder.etag = "ETAG_OLD"
        Self.dbManager.addItemMetadata(unmaterialisedFolder)

        let remoteUnmaterialisedFolderChild = MockRemoteItem(
            identifier: "unmaterialised-folder-child",
            versionIdentifier: "NEW",
            name: "unmaterialised folder child",
            remotePath: remoteUnmaterialisedFolder.remotePath + "/" + "unmaterialised folder child",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        remoteUnmaterialisedFolderChild.parent = remoteUnmaterialisedFolder
        remoteUnmaterialisedFolder.children.append(remoteUnmaterialisedFolderChild)

        // A materialised file inside the folder that will be updated on the server.
        var fileToUpdate = remoteItemA.toItemMetadata(account: Self.account)
        fileToUpdate.downloaded = true
        fileToUpdate.etag = "ETAG_OLD"
        Self.dbManager.addItemMetadata(fileToUpdate)

        // A materialised file inside the folder that will be deleted from the server.
        var fileToBeDeleted = remoteItemB.toItemMetadata(account: Self.account)
        fileToBeDeleted.downloaded = true
        Self.dbManager.addItemMetadata(fileToBeDeleted)

        // A top-level materialised file that does not exist remotely (i.e. deleted), will cause 404
        var topLevelFileToBeDeleted = SendableItemMetadata(
            ocId: "toplevel-deleted", fileName: "toplevel-deleted.txt", account: Self.account
        )
        topLevelFileToBeDeleted.downloaded = true
        topLevelFileToBeDeleted.uploaded = true
        Self.dbManager.addItemMetadata(topLevelFileToBeDeleted)

        // --- Server State (The "remote truth" for the test) ---
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        // Update server state to reflect the changes
        remoteItemA.versionIdentifier = "ETAG_NEW"

        // Add a new file on the server inside the materialised folder
        let newChildFile = MockRemoteItem(
            identifier: "new-child",
            name: "new-child.txt",
            remotePath: remoteFolder.remotePath + "/new-child.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        newChildFile.parent = remoteFolder
        remoteFolder.children.append(newChildFile)

        // Remove the "deleted" file from the server
        remoteFolder.children.removeAll(where: { $0.identifier == fileToBeDeleted.ocId })

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)

        // 2. Act
        try await observer.enumerateChanges()

        // 3. Assert
        XCTAssertNil(observer.error, "Enumeration should complete without error.")

        // Verify Deletions
        let deletedIds = observer.deletedItemIdentifiers.map(\.rawValue)
        XCTAssertEqual(deletedIds.count, 2, "There should be two deleted items.")
        XCTAssertTrue(
            deletedIds.contains(fileToBeDeleted.ocId),
            "The file deleted from the server folder should be reported as deleted."
        )
        XCTAssertTrue(
            deletedIds.contains(topLevelFileToBeDeleted.ocId),
            "The top-level file that resulted in a 404 should be reported as deleted."
        )

        // Verify Updates and Additions
        let changedIds = observer.changedItems.map(\.itemIdentifier.rawValue)
        print(changedIds)
        XCTAssertEqual(
            changedIds.count,
            5,
            "There should be one updated file, three updated folders, and one new file."
        )
        XCTAssertTrue(changedIds.contains(NSFileProviderItemIdentifier.rootContainer.rawValue))
        XCTAssertTrue(changedIds.contains(materialisedFolder.ocId))
        XCTAssertTrue(changedIds.contains(unmaterialisedFolder.ocId))
        XCTAssertTrue(changedIds.contains(fileToUpdate.ocId))
        XCTAssertTrue(changedIds.contains(newChildFile.identifier))

        // Verify the database state was updated
        let finalUpdatedFile = Self.dbManager.itemMetadata(ocId: fileToUpdate.ocId)
        XCTAssertEqual(
            finalUpdatedFile?.etag,
            "ETAG_NEW",
            "The ETag for the updated file should be changed in the database."
        )

        let finalNewFile = Self.dbManager.itemMetadata(ocId: newChildFile.identifier)
        XCTAssertNotNil(finalNewFile, "The new file should now exist in the database.")
    }

    func testFolderEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        let oldEtag = "OLD"
        var folderMetadata = remoteFolder.toItemMetadata(account: Self.account)
        folderMetadata.etag = oldEtag

        Self.dbManager.addItemMetadata(folderMetadata)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 2)

        // A pass of enumerating a target should update the target too. Let's check.
        let dbFolderMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteFolder.identifier)
        )
        let storedFolderItemMaybe = await Item.storedItem(
            identifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
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
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

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
            dbManager: Self.dbManager
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
            dbManager: Self.dbManager
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
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

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
            dbManager: Self.dbManager
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
        let dbItemCMetadata = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: remoteItemC.identifier)
        )
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteItemB.identifier))
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
            dbManager: Self.dbManager
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
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

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
            dbManager: Self.dbManager
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
            dbManager: Self.dbManager
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

        let storedRootItem = Item.rootContainer(
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            remoteSupportsTrash: await remoteInterface.supportsTrash(account: Self.account)
        )
        print(storedRootItem.metadata.serverUrl)
        XCTAssertEqual(storedRootItem.childItemCount?.intValue, 4) // All items

        let storedFolderMaybe = await Item.storedItem(
            identifier: .init(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let storedFolder = try XCTUnwrap(storedFolderMaybe)
        XCTAssertEqual(storedFolder.childItemCount?.intValue, remoteFolder.children.count)
    }

    func testFileLockStateEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

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
            dbManager: Self.dbManager
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
            dbManager: Self.dbManager
        )
        let storedItemA = try XCTUnwrap(storedItemAMaybe)
        let storedItemBMaybe = await Item.storedItem(
            identifier: .init(remoteItemB.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let storedItemB = try XCTUnwrap(storedItemBMaybe)
        let storedItemCMaybe = await Item.storedItem(
            identifier: .init(remoteItemC.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let storedItemC = try XCTUnwrap(storedItemCMaybe)

        // Should be able to write to files locked by self
        XCTAssertTrue(storedItemA.fileSystemFlags.contains(.userWritable))
        // Should not be able to write to files locked by someone else
        XCTAssertFalse(storedItemB.fileSystemFlags.contains(.userWritable))
        // Should be able to write to files with an expired lock
        XCTAssertTrue(storedItemC.fileSystemFlags.contains(.userWritable))
    }

    // File Provider system will panic if we give it an NSFileProviderItem with an empty filename.
    // Test that we have a fallback to avoid this, even if something catastrophic happens in the
    // server and the file has no filename
    func testEnsureNoEmptyItemNameEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        remoteItemA.name = ""
        remoteItemA.parent = remoteInterface.rootItem
        rootItem.children = [remoteItemA]

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
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
            dbManager: Self.dbManager
        )
        let storedItemA = try XCTUnwrap(storedItemAMaybe)
        XCTAssertEqual(storedItemA.itemIdentifier.rawValue, remoteItemA.identifier)
        XCTAssertNotEqual(storedItemA.filename, remoteItemA.name)
        XCTAssertFalse(storedItemA.filename.isEmpty)
    }

    func testTrashEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 3)

        let storedItemAMaybe = await Item.storedItem(
            identifier: .init(remoteTrashItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let storedItemA = try XCTUnwrap(storedItemAMaybe)
        XCTAssertEqual(storedItemA.itemIdentifier.rawValue, remoteTrashItemA.identifier)
        XCTAssertEqual(storedItemA.filename, remoteTrashItemA.name)
        XCTAssertEqual(storedItemA.documentSize?.int64Value, remoteTrashItemA.size)
        XCTAssertEqual(storedItemA.isDownloaded, false)
        XCTAssertEqual(storedItemA.isUploaded, true)

        let storedItemBMaybe = await Item.storedItem(
            identifier: .init(remoteTrashItemB.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let storedItemB = try XCTUnwrap(storedItemBMaybe)
        XCTAssertEqual(storedItemB.itemIdentifier.rawValue, remoteTrashItemB.identifier)
        XCTAssertEqual(storedItemB.filename, remoteTrashItemB.name)
        XCTAssertEqual(storedItemB.documentSize?.int64Value, remoteTrashItemB.size)
        XCTAssertEqual(storedItemB.isDownloaded, false)
        XCTAssertEqual(storedItemB.isUploaded, true)

        let storedItemCMaybe = await Item.storedItem(
            identifier: .init(remoteTrashItemC.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let storedItemC = try XCTUnwrap(storedItemCMaybe)
        XCTAssertEqual(storedItemC.itemIdentifier.rawValue, remoteTrashItemC.identifier)
        XCTAssertEqual(storedItemC.filename, remoteTrashItemC.name)
        XCTAssertEqual(storedItemC.documentSize?.int64Value, remoteTrashItemC.size)
        XCTAssertEqual(storedItemC.isDownloaded, false)
        XCTAssertEqual(storedItemC.isUploaded, true)
    }

    func testTrashChangeEnumeration() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
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
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 0)
        observer.reset()

        rootTrashItem.children = [remoteTrashItemA, remoteTrashItemB]
        remoteTrashItemB.parent = rootTrashItem
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 1)
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemB.identifier))
        observer.reset()

        rootTrashItem.children = [remoteTrashItemB, remoteTrashItemC]
        remoteTrashItemA.parent = nil
        remoteTrashItemC.parent = rootTrashItem
        try await observer.enumerateChanges()
        XCTAssertEqual(observer.changedItems.count, 1)
        XCTAssertEqual(observer.deletedItemIdentifiers.count, 1)
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemA.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemB.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteTrashItemC.identifier))
    }

    func testTrashItemEnumerationFailWhenNoTrashInCapabilities() async {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""undelete": true,"##, with: "")

        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        do {
            try await observer.enumerateItems()
            XCTFail("Item enumeration should have failed!")
        } catch let error {
            XCTAssertEqual((error as NSError?)?.code, NSFeatureUnsupportedError)
        }
    }

    func testTrashChangeEnumerationFailWhenNoTrashInCapabilities() async {
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)
        XCTAssert(remoteInterface.capabilities.contains(##""undelete": true,"##))
        remoteInterface.capabilities =
            remoteInterface.capabilities.replacingOccurrences(of: ##""undelete": true,"##, with: "")

        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let enumerator = Enumerator(
            enumeratedItemIdentifier: .trashContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        do {
            try await observer.enumerateChanges()
            XCTFail("Item enumeration should have failed!")
        } catch let error {
            XCTAssertEqual((error as NSError?)?.code, NSFeatureUnsupportedError)
        }
    }

    func testRemoteLockFilesNotEnumerated() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db) // Avoid build-time warning about unused variable, ensure compiler won't free
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, rootTrashItem: rootTrashItem)

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
            dbManager: Self.dbManager
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 1)
        XCTAssertFalse(
            observer.items.contains(where: { $0.itemIdentifier.rawValue == "lock-file" })
        )
    }

    // Tests situation where we are enumerating files and we can no longer find the parent item
    // in the database. So we need to simulate a situation where this takes place.
    func testCorrectEnumerateFileWithMissingParentInDb() async throws {
        let db = Self.dbManager.ncDatabase() // Strong ref for in memory test db
        debugPrint(db)
        let remoteInterface = MockRemoteInterface(rootItem: rootItem)

        var itemAMetadata = remoteItemA.toItemMetadata(account: Self.account)
        itemAMetadata.etag = "OLD"

        Self.dbManager.addItemMetadata(itemAMetadata)
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: remoteFolder.identifier))
        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: remoteItemA.identifier))

        let enumerator = Enumerator(
            enumeratedItemIdentifier: .init(remoteItemA.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
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
            dbManager: Self.dbManager
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
        for i in 0...20 {
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
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, pagination: true)

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
            pageSize: 5
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItems()
        XCTAssertEqual(observer.items.count, 21)

        for item in observer.items {
            XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue))
        }
        XCTAssertEqual(
            observer.items.filter { $0.contentType?.conforms(to: .folder) ?? false }.count,
            5
        )
        XCTAssertTrue(observer.items.last?.contentType?.conforms(to: .folder) ?? false)

        XCTAssertEqual(observer.observedPages.first, NSFileProviderPage.initialPageSortedByName as NSFileProviderPage)
        XCTAssertEqual(observer.observedPages.count, 5)
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
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, pagination: true)

        // 2. Create enumerator for the empty folder with a specific pageSize.
        let enumerator = Enumerator(
            enumeratedItemIdentifier: NSFileProviderItemIdentifier(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            pageSize: 5 // Page size can be anything, as the folder is empty
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)

        // 3. Enumerate items.
        try await observer.enumerateItems()

        // 4. Assertions.
        // When enumerating a folder (even an empty one) with depth .targetAndDirectChildren,
        // the folder item itself should not be returned.
        XCTAssertEqual(observer.items.count, 0, "Should enumerate nothing.")

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
        for i in 0..<3 {
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
        let remoteInterface = MockRemoteInterface(rootItem: rootItem, pagination: true)

        // 2. Create enumerator with pageSize > number of children.
        let enumerator = Enumerator(
            enumeratedItemIdentifier: NSFileProviderItemIdentifier(remoteFolder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            pageSize: 5
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)

        // 3. Enumerate items.
        try await observer.enumerateItems()

        // 4. Assertions.
        // Expected items: 0 (folder itself) + 3 children = 3 items.
        XCTAssertEqual(observer.items.count, 3, "Should enumerate the 3 children.")
        XCTAssertFalse(
            observer.items.contains(where: { $0.itemIdentifier.rawValue == remoteFolder.identifier }),
            "Folder itself should be enumerated."
        )
        for i in 0..<3 {
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
        for i in 0..<3 {
             XCTAssertNotNil(
                Self.dbManager.itemMetadata(ocId: "fewItems-child\(i)"),
                "Child item fewItems-child\(i) metadata should be in DB."
             )
        }
    }
}
