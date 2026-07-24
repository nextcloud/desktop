//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import TestInterface
import XCTest

final class FilesDatabaseManagerTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    func testFilesDatabaseManagerInitialization() {
        XCTAssertNotNil(Self.dbManager, "FilesDatabaseManager should be initialized")
    }

    func testContainsAnyItemMetadataFileIds() throws {
        let metadata = RealmItemMetadata()
        metadata.ocId = UUID().uuidString
        metadata.fileId = "11915767"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        XCTAssertTrue(Self.dbManager.containsAnyItemMetadata(fileIds: ["11915767", "13347012"]))
        XCTAssertFalse(Self.dbManager.containsAnyItemMetadata(fileIds: ["13347012"]))
    }

    /// `ancestorContainerIdentifiers` returns every directory on a file's path to
    /// the root (root excluded) — the set of folders whose "Remove download"
    /// visibility must be refreshed when that file materializes (#10085).
    func testAncestorContainerIdentifiersForMaterializedFile() throws {
        let folder = RealmItemMetadata()
        folder.ocId = "folder-1"
        folder.account = Self.account.ncKitAccount
        folder.serverUrl = "https://cloud.example.com/files"
        folder.fileName = "documents"
        folder.directory = true

        let subfolder = RealmItemMetadata()
        subfolder.ocId = "subfolder-1"
        subfolder.account = Self.account.ncKitAccount
        subfolder.serverUrl = "https://cloud.example.com/files/documents"
        subfolder.fileName = "nested"
        subfolder.directory = true

        let deepFile = RealmItemMetadata()
        deepFile.ocId = "deep-file"
        deepFile.account = Self.account.ncKitAccount
        deepFile.serverUrl = "https://cloud.example.com/files/documents/nested"
        deepFile.fileName = "note.txt"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(folder)
            realm.add(subfolder)
            realm.add(deepFile)
        }

        // Every directory on the deep file's path, root excluded.
        XCTAssertEqual(
            Self.dbManager.ancestorContainerIdentifiers(ofFileItemsWithOcIds: ["deep-file"]),
            [NSFileProviderItemIdentifier("subfolder-1"), NSFileProviderItemIdentifier("folder-1")]
        )

        // Directory ocIds are ignored — only a file's materialization feeds an
        // ancestor's displayEvict.
        XCTAssertTrue(
            Self.dbManager.ancestorContainerIdentifiers(ofFileItemsWithOcIds: ["subfolder-1"]).isEmpty
        )

        // Unknown ocIds are skipped without error.
        XCTAssertTrue(
            Self.dbManager.ancestorContainerIdentifiers(ofFileItemsWithOcIds: ["does-not-exist"]).isEmpty
        )
    }

    /// A file directly under the account root resolves its parent to the root
    /// container, which must be included so "Remove download" is offered on the
    /// whole file provider — symmetric with pinning it all offline (#10085).
    func testAncestorContainerIdentifiersIncludeRootContainer() throws {
        let topFile = RealmItemMetadata()
        topFile.ocId = "top-file"
        topFile.account = Self.account.ncKitAccount
        topFile.serverUrl = Self.account.davFilesUrl
        topFile.urlBase = Self.account.serverUrl
        topFile.userId = Self.account.id
        topFile.fileName = "top.txt"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(topFile)
        }

        XCTAssertEqual(
            Self.dbManager.ancestorContainerIdentifiers(ofFileItemsWithOcIds: ["top-file"]),
            [.rootContainer]
        )
    }

    /// `evictableDescendantFileIdentifiers` returns exactly the downloaded,
    /// non-deleted, non-pinned descendant FILES — the set the "Remove downloaded
    /// items" handler evicts individually (#10085). Pinned, dataless, tombstoned,
    /// and directory rows are excluded; deep descendants are included.
    func testEvictableDescendantFileIdentifiers() throws {
        let base = "https://cloud.example.com/files"
        let dir = base + "/documents"

        func seed(_ ocId: String, _ name: String, url: String, directory: Bool = false, downloaded: Bool = false, deleted: Bool = false, keepDownloaded: Bool = false) -> RealmItemMetadata {
            let m = RealmItemMetadata()
            m.ocId = ocId
            m.account = Self.account.ncKitAccount
            m.serverUrl = url
            m.fileName = name
            m.directory = directory
            m.downloaded = downloaded
            m.deleted = deleted
            m.keepDownloaded = keepDownloaded
            return m
        }

        let rows = [
            seed("folder-1", "documents", url: base, directory: true),
            seed("file-a", "a.txt", url: dir, downloaded: true), // evictable
            seed("file-b", "b.txt", url: dir, downloaded: true, keepDownloaded: true), // pinned -> excluded
            seed("file-c", "c.txt", url: dir, downloaded: false), // dataless -> excluded
            seed("file-d", "d.txt", url: dir, downloaded: true, deleted: true), // tombstone -> excluded
            seed("subfolder", "nested", url: dir, directory: true, downloaded: true), // directory -> excluded
            seed("deep-file", "deep.txt", url: dir + "/nested", downloaded: true) // evictable (deep)
        ]

        let realm = Self.dbManager.ncDatabase()
        try realm.write { rows.forEach { realm.add($0) } }

        let folderMeta = try XCTUnwrap(Self.dbManager.directoryMetadata(ocId: "folder-1"))
        let ids = Set(Self.dbManager.evictableDescendantFileIdentifiers(directoryMetadata: folderMeta).map(\.rawValue))
        XCTAssertEqual(ids, ["file-a", "deep-file"])
    }

    func testAnyItemMetadatasForAccount() throws {
        // Insert test data
        let expected = true
        let testAccount = "TestAccount"
        let metadata = RealmItemMetadata()
        metadata.account = testAccount

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        // Perform test
        let result = Self.dbManager.anyItemMetadatasForAccount(testAccount)
        XCTAssertEqual(
            result,
            expected,
            "anyItemMetadatasForAccount should return \(expected) for existing account"
        )
    }

    func testItemMetadataFromOcId() throws {
        let ocId = "unique-id-123"
        let metadata = RealmItemMetadata()
        metadata.ocId = ocId

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        let fetchedMetadata = Self.dbManager.itemMetadata(ocId: ocId)
        XCTAssertNotNil(fetchedMetadata, "Should fetch metadata with the specified ocId")
        XCTAssertEqual(
            fetchedMetadata?.ocId, ocId, "Fetched metadata ocId should match the requested ocId"
        )
    }

    func testUpdateItemMetadatas() {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")
        var metadata = SendableItemMetadata(ocId: "test", fileName: "test", account: account)
        metadata.downloaded = true
        metadata.uploaded = true

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [metadata],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result?.created.isEmpty, false, "Should create new metadatas")
        XCTAssertEqual(result?.updated.isEmpty, true, "No metadata should be updated")

        // Now test we are receiving the updated basic metadatas correctly.
        metadata.etag = "new and shiny"

        let result2 = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [metadata],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result2?.created.isEmpty, true, "Should create no new metadatas")
        XCTAssertEqual(result2?.updated.isEmpty, false, "Metadata should be updated")

        // Also check the download state is correctly kept the same.
        // We set it to false here to replicate the lack of a download state when converting from
        // the NKFiles received during remote enumeration
        metadata.downloaded = false
        metadata.etag = "new and shiny, but keeping original download state"

        let result3 = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [metadata],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result3?.created.isEmpty, true, "Should create no new metadatas")
        XCTAssertEqual(result3?.updated.isEmpty, false, "Metadata should be updated")
        XCTAssertEqual(result3?.updated.first?.downloaded, true)
    }

    func testUpdateRenamesDirectoryAndPropagatesToChildren() {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        var rootMetadata = SendableItemMetadata(ocId: "root", fileName: "", account: Self.account)
        rootMetadata.directory = true
        Self.dbManager.addItemMetadata(rootMetadata)

        // Insert original parent directory
        var parent = SendableItemMetadata(ocId: "dir1", fileName: "oldDir", account: account)
        parent.directory = true
        parent.serverUrl = account.davFilesUrl
        parent.downloaded = true
        parent.uploaded = true
        Self.dbManager.addItemMetadata(parent)

        // Insert a child item inside that directory
        var child = SendableItemMetadata(ocId: "child1", fileName: "file.txt", account: account)
        child.serverUrl = account.davFilesUrl + "/oldDir"
        child.downloaded = true
        Self.dbManager.addItemMetadata(child)

        var newContainerFolder = SendableItemMetadata(
            ocId: "ncf", fileName: "newContainerFolder", account: account
        )
        newContainerFolder.serverUrl = account.davFilesUrl
        newContainerFolder.downloaded = true
        Self.dbManager.addItemMetadata(newContainerFolder)

        // Rename the directory
        var renamedParent = parent
        renamedParent.fileName = "newDir"
        renamedParent.serverUrl = account.davFilesUrl + "/" + newContainerFolder.fileName
        renamedParent.etag = "etag-changed"

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [rootMetadata, renamedParent],
            keepExistingDownloadState: true
        )

        // Ensure rename took place
        XCTAssertEqual(result?.created.isEmpty, true)
        XCTAssertEqual(result?.updated.isEmpty, false)
        XCTAssertNotNil(result?.updated.first(where: { $0.fileName == "newDir" }))

        // Ensure the child's serverUrl was updated accordingly
        let updatedChild = Self.dbManager.itemMetadata(ocId: "child1")
        XCTAssertNotNil(updatedChild)
        XCTAssertEqual(updatedChild?.serverUrl, account.davFilesUrl + "/newContainerFolder/newDir")
    }

    func testTransitItemIsNotUpdated() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Simulate existing item in transit
        var transit = SendableItemMetadata(ocId: "transit1", fileName: "temp", account: account)
        transit.uploaded = true
        transit.downloaded = false
        transit.status = Status.downloading.rawValue
        transit.etag = "old-etag"
        Self.dbManager.addItemMetadata(transit)

        // Send an updated version of the same item
        var incoming = transit
        incoming.uploaded = true
        incoming.downloaded = false
        incoming.status = Status.normal.rawValue
        incoming.etag = "new-etag"

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [incoming],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result?.updated.isEmpty, true, "Transit items should not be updated")
        XCTAssertEqual(result?.created.isEmpty, true)
        XCTAssertEqual(result?.deleted.isEmpty, true)

        let inDb = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: transit.ocId))
        XCTAssertEqual(inDb.etag, transit.etag)
    }

    func testTransitItemIsDeleted() {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Simulate existing item in transit
        var transit = SendableItemMetadata(ocId: "transit1", fileName: "temp", account: account)
        transit.uploaded = true
        transit.downloaded = false
        transit.status = Status.downloading.rawValue
        Self.dbManager.addItemMetadata(transit)

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result?.updated.isEmpty, true)
        XCTAssertEqual(result?.created.isEmpty, true)
        XCTAssertEqual(result?.deleted.isEmpty, false)
        XCTAssertEqual(result?.deleted.first?.ocId, transit.ocId)
    }

    func testSetStatusForItemMetadata() throws {
        // Create and add a test metadata to the database
        let metadata = RealmItemMetadata()
        metadata.ocId = "unique-id-123"
        metadata.status = Status.normal.rawValue

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        let expectedStatus = Status.uploadError
        let updatedMetadata = Self.dbManager.setStatusForItemMetadata(
            SendableItemMetadata(value: metadata), status: expectedStatus
        )
        XCTAssertEqual(
            updatedMetadata?.status,
            expectedStatus.rawValue,
            "Status should be updated to \(expectedStatus)"
        )
    }

    func testAddItemMetadata() {
        let metadata = SendableItemMetadata(
            ocId: "unique-id-123",
            fileName: "b",
            account: .init(user: "t", id: "t", serverUrl: "b", password: "")
        )
        Self.dbManager.addItemMetadata(metadata)

        let fetchedMetadata = Self.dbManager.itemMetadata(ocId: "unique-id-123")
        XCTAssertNotNil(fetchedMetadata, "Metadata should be added to the database")
    }

    func testDeleteItemMetadata() throws {
        let ocId = "unique-id-123"
        let metadata = RealmItemMetadata()
        metadata.ocId = ocId

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        let result = Self.dbManager.deleteItemMetadata(ocId: ocId)
        XCTAssertTrue(result, "deleteItemMetadata should return true on successful deletion")
        XCTAssertEqual(
            Self.dbManager.itemMetadata(ocId: ocId)?.deleted,
            true,
            "Metadata should be deleted from the database"
        )
    }

    func testRenameItemMetadata() throws {
        let ocId = "unique-id-123"
        let newFileName = "newFileName.pdf"
        let newServerUrl = "https://new.example.com"
        let metadata = RealmItemMetadata()
        metadata.ocId = ocId
        metadata.fileName = "oldFileName.pdf"
        metadata.serverUrl = "https://old.example.com"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        Self.dbManager.renameItemMetadata(
            ocId: ocId, newServerUrl: newServerUrl, newFileName: newFileName
        )

        let updatedMetadata = Self.dbManager.itemMetadata(ocId: ocId)
        XCTAssertEqual(updatedMetadata?.fileName, newFileName, "File name should be updated")
        XCTAssertEqual(updatedMetadata?.serverUrl, newServerUrl, "Server URL should be updated")
    }

    func testDeleteItemMetadatasBasedOnUpdate() throws {
        // Existing metadata in the database
        let existingMetadata1 = RealmItemMetadata()
        existingMetadata1.ocId = "id-1"
        existingMetadata1.fileName = "Existing.pdf"
        existingMetadata1.serverUrl = "https://example.com"
        existingMetadata1.account = "TestAccount"
        existingMetadata1.downloaded = true
        existingMetadata1.uploaded = true

        let existingMetadata2 = RealmItemMetadata()
        existingMetadata2.ocId = "id-2"
        existingMetadata2.fileName = "Existing2.pdf"
        existingMetadata2.serverUrl = "https://example.com"
        existingMetadata2.account = "TestAccount"
        existingMetadata2.downloaded = true
        existingMetadata2.uploaded = true

        let existingMetadata3 = RealmItemMetadata()
        existingMetadata3.ocId = "id-3"
        existingMetadata3.fileName = "Existing3.pdf"
        existingMetadata3.serverUrl = "https://example.com/folder" // Different child path
        existingMetadata3.account = "TestAccount"
        existingMetadata3.downloaded = true
        existingMetadata3.uploaded = true

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(existingMetadata1)
            realm.add(existingMetadata2)
            realm.add(existingMetadata3)
        }

        // Simulate updated metadata that leads to a deletion
        let updatedMetadatas = [existingMetadata1, existingMetadata3] // Only include 2 of the 3

        _ = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: "TestAccount",
            serverUrl: "https://example.com",
            updatedMetadatas: updatedMetadatas.map { SendableItemMetadata(value: $0) },
            keepExistingDownloadState: true
        )

        let remainingMetadatas = Self.dbManager.itemMetadatas(
            account: "TestAccount", underServerUrl: "https://example.com"
        )
        XCTAssertEqual(remainingMetadatas.filter(\.deleted).count, 1)
        XCTAssertEqual(remainingMetadatas.count(where: { !$0.deleted }), 2)

        XCTAssertNotNil(remainingMetadatas.first { $0.ocId == "id-1" })
        XCTAssertNotNil(remainingMetadatas.first { $0.ocId == "id-3" })
    }

    func testProcessItemMetadatasToUpdate_NewAndUpdatedSeparation() throws {
        let account = Account(
            user: "TestAccount", id: "taid", serverUrl: "https://example.com", password: "pass"
        )

        let parent = RealmItemMetadata()
        parent.ocId = "parent"
        parent.fileName = "Parent"
        parent.account = "TestAccount"
        parent.serverUrl = "https://example.com"
        parent.directory = true
        parent.downloaded = true
        parent.uploaded = true

        // Simulate existing metadata in the database
        let existingMetadata = RealmItemMetadata()
        existingMetadata.ocId = "id-1"
        existingMetadata.fileName = "File.pdf"
        existingMetadata.account = "TestAccount"
        existingMetadata.serverUrl = "https://example.com/Parent"
        existingMetadata.downloaded = true
        existingMetadata.uploaded = true

        // Simulate updated metadata that includes changes and a new entry
        var updatedParent = SendableItemMetadata(ocId: "parent", fileName: "Parent", account: account)
        updatedParent.directory = true

        var updatedMetadata =
            SendableItemMetadata(ocId: "id-1", fileName: "UpdatedFile.pdf", account: account)
        updatedMetadata.serverUrl = "https://example.com/Parent"

        var newMetadata =
            SendableItemMetadata(ocId: "id-2", fileName: "NewFile.pdf", account: account)
        newMetadata.serverUrl = "https://example.com/Parent"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(parent)
            realm.add(existingMetadata)
        }

        let results = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: "TestAccount",
            serverUrl: "https://example.com/Parent",
            updatedMetadatas: [updatedParent, updatedMetadata, newMetadata],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(results?.created.count, 1, "Should create one new metadata")
        XCTAssertEqual(results?.updated.count, 2, "Should update two existing metadata")
        XCTAssertEqual(
            results?.created.first?.ocId, "id-2", "New metadata ocId should be 'id-2'"
        )
        XCTAssertEqual(
            results?.updated.last?.fileName,
            "UpdatedFile.pdf",
            "Updated metadata should have the new file name"
        )
    }

    func testUnuploadedItemsAreNotDeletedDuringUpdate() throws {
        let testAccount = Self.account.ncKitAccount
        let testServerUrl = Self.account.davFilesUrl
        // 1. Item that exists locally and is marked as uploaded
        let uploadedItem = RealmItemMetadata()
        uploadedItem.ocId = "ocid-uploaded-123"
        uploadedItem.fileName = "SyncedFile.txt"
        uploadedItem.account = testAccount
        uploadedItem.serverUrl = testServerUrl
        uploadedItem.downloaded = true
        uploadedItem.uploaded = true // IMPORTANT: Marked as uploaded

        // 2. Item that exists locally but is NOT marked as uploaded (e.g., new local file)
        let unuploadedItem = RealmItemMetadata()
        unuploadedItem.ocId = "ocid-local-456" // May or may not have ocId yet
        unuploadedItem.fileName = "NewLocalFile.txt"
        unuploadedItem.account = testAccount
        unuploadedItem.serverUrl = testServerUrl
        unuploadedItem.downloaded = true
        unuploadedItem.uploaded = false // IMPORTANT: Not marked as uploaded
        unuploadedItem.status = Status.normal.rawValue // Ensure it's not in a transient state if relevant

        var rootMetadata = SendableItemMetadata(ocId: "root", fileName: "", account: Self.account)
        rootMetadata.directory = true

        Self.dbManager.addItemMetadata(rootMetadata)

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(uploadedItem)
            realm.add(unuploadedItem)
        }

        XCTAssertEqual(realm.objects(RealmItemMetadata.self).where {
            $0.account == testAccount && $0.serverUrl == testServerUrl
        }.count, 3)

        // Simulate an update from the server that contains NEITHER of these items.
        // This means the server thinks 'SyncedFile.txt' was deleted,
        // and it doesn't know about 'NewLocalFile.txt' yet.
        let updatedMetadatasFromServer = [rootMetadata]

        let results = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: testAccount,
            serverUrl: testServerUrl,
            updatedMetadatas: updatedMetadatasFromServer,
            keepExistingDownloadState: true // Value doesn't strictly matter for this test logic
        )

        // --- Assertion ---
        let remainingMetadatas = realm.objects(RealmItemMetadata.self)
            .where { $0.account == testAccount && $0.serverUrl == testServerUrl }

        // Check the returned delete list (based on the copy made before deletion)
        XCTAssertEqual(results?.deleted.count, 1, "Should identify the uploaded item as deleted.")
        XCTAssertEqual(results?.deleted.first?.ocId, "ocid-uploaded-123", "The correct uploaded item should be marked for deletion.")
        XCTAssertTrue(results?.deleted.first?.uploaded ?? false, "The item marked for deletion should have uploaded=true")

        // Check the actual database state after the write transaction
        XCTAssertEqual(remainingMetadatas.filter(\.deleted).count, 1)
        XCTAssertEqual(remainingMetadatas.count(where: { !$0.deleted }), 2)

        let survivingItem = remainingMetadatas.last
        XCTAssertNotNil(survivingItem, "An item should survive.")
        XCTAssertEqual(survivingItem?.ocId, "ocid-local-456", "The surviving item should be the unuploaded one.")
        XCTAssertEqual(survivingItem?.fileName, "NewLocalFile.txt", "Filename should match the unuploaded item.")
        XCTAssertFalse(try XCTUnwrap(survivingItem?.uploaded), "The surviving item must be the one marked uploaded = false.")

        // Check other return values are empty as expected
        XCTAssertTrue(results?.created.isEmpty ?? true, "No new items should have been created.")
        XCTAssertTrue(results?.updated.isEmpty ?? true, "No items should have been updated.")
    }

    func testDirectoryMetadataRetrieval() throws {
        let account = "TestAccount"
        let serverUrl = "https://cloud.example.com/files/documents"
        let directoryFileName = "documents"
        let metadata = RealmItemMetadata()
        metadata.ocId = "dir-1"
        metadata.account = account
        metadata.serverUrl = "https://cloud.example.com/files"
        metadata.fileName = directoryFileName
        metadata.directory = true

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        let retrievedMetadata = Self.dbManager.itemMetadata(
            account: account, locatedAtRemoteUrl: serverUrl
        )
        XCTAssertNotNil(retrievedMetadata, "Should retrieve directory metadata")
        XCTAssertEqual(
            retrievedMetadata?.fileName, directoryFileName, "Should match the directory file name"
        )
    }

    func testChildItemsForDirectory() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-1"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "documents"
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "item-1"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/documents"
        childMetadata.fileName = "report.pdf"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
        }

        let children = Self.dbManager.childItems(
            directoryMetadata: SendableItemMetadata(value: directoryMetadata)
        )
        XCTAssertEqual(children.count, 1, "Should return one child item")
        XCTAssertEqual(
            children.first?.fileName, "report.pdf", "Should match the child item's file name"
        )
    }

    func testDeleteDirectoryAndSubdirectoriesMetadata() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-1"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "documents"
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "item-1"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/documents"
        childMetadata.fileName = "report.pdf"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
        }

        let deletedMetadatas = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(
            ocId: "dir-1"
        )
        XCTAssertNotNil(deletedMetadatas, "Should return a list of deleted metadatas")
        XCTAssertEqual(deletedMetadatas?.count, 2, "Should delete the directory and its child")
    }

    func testRenameDirectoryAndPropagateToChildren() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-1"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "documents"
        directoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "item-1"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/documents"
        childMetadata.fileName = "report.pdf"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
            realm.add(childMetadata)
        }

        let updatedChildren = Self.dbManager.renameDirectoryAndPropagateToChildren(
            ocId: "dir-1",
            newServerUrl: "https://cloud.example.com/office",
            newFileName: "files"
        )

        XCTAssertNotNil(updatedChildren, "Should return updated children metadatas")
        XCTAssertEqual(updatedChildren?.count, 1, "Should include one child")
        XCTAssertEqual(
            updatedChildren?.first?.serverUrl,
            "https://cloud.example.com/office/files",
            "Should update serverUrl of child items"
        )
    }

    func testErrorOnDirectoryMetadataNotFound() {
        let nonExistentServerUrl = "https://cloud.example.com/nonexistent"
        let directoryMetadata = Self.dbManager.itemMetadata(
            account: "TestAccount", locatedAtRemoteUrl: nonExistentServerUrl
        )
        XCTAssertNil(directoryMetadata, "Should return nil when directory metadata is not found")
    }

    func testChildItemsForRootDirectory() throws {
        let rootMetadata = SendableItemMetadata(
            ocId: NSFileProviderItemIdentifier.rootContainer.rawValue,
            fileName: "",
            account: Account(
                user: "TestAccount",
                id: "ta",
                serverUrl: "https://cloud.example.com/files",
                password: ""
            )
        ) // Do not write, we do not track root container

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "item-1"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = rootMetadata.serverUrl
        childMetadata.fileName = "report.pdf"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(childMetadata)
        }

        let children = Self.dbManager.childItems(directoryMetadata: rootMetadata)
        XCTAssertEqual(children.count, 1, "Should return one child item for the root directory")
        XCTAssertEqual(
            children.first?.fileName,
            "report.pdf",
            "Should match the child item's file name for root directory"
        )
    }

    func testDeleteNestedDirectoriesAndSubdirectoriesMetadata() throws {
        // Create nested directories and their child items
        let rootDirectoryMetadata = RealmItemMetadata()
        rootDirectoryMetadata.ocId = "dir-1"
        rootDirectoryMetadata.account = "TestAccount"
        rootDirectoryMetadata.serverUrl = "https://cloud.example.com/files"
        rootDirectoryMetadata.fileName = "documents"
        rootDirectoryMetadata.directory = true

        let nestedDirectoryMetadata = RealmItemMetadata()
        nestedDirectoryMetadata.ocId = "dir-2"
        nestedDirectoryMetadata.account = "TestAccount"
        nestedDirectoryMetadata.serverUrl = "https://cloud.example.com/files/documents"
        nestedDirectoryMetadata.fileName = "projects"
        nestedDirectoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "item-1"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/documents/projects"
        childMetadata.fileName = "report.pdf"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(rootDirectoryMetadata)
            realm.add(nestedDirectoryMetadata)
            realm.add(childMetadata)
        }

        let deletedMetadatas = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(
            ocId: "dir-1"
        )
        XCTAssertNotNil(deletedMetadatas, "Should return a list of deleted metadatas")
        XCTAssertEqual(
            deletedMetadatas?.count,
            3,
            "Should delete the root directory, nested directory, and its child"
        )
    }

    func testRecursiveRenameOfDirectoriesAndChildItems() throws {
        // Setup a complex directory structure
        let rootDirectoryMetadata = RealmItemMetadata()
        rootDirectoryMetadata.ocId = "dir-1"
        rootDirectoryMetadata.account = "TestAccount"
        rootDirectoryMetadata.serverUrl = "https://cloud.example.com/files"
        rootDirectoryMetadata.fileName = "documents"
        rootDirectoryMetadata.directory = true

        let nestedDirectoryMetadata = RealmItemMetadata()
        nestedDirectoryMetadata.ocId = "dir-2"
        nestedDirectoryMetadata.account = "TestAccount"
        nestedDirectoryMetadata.serverUrl = "https://cloud.example.com/files/documents"
        nestedDirectoryMetadata.fileName = "projects"
        nestedDirectoryMetadata.directory = true

        let childMetadata = RealmItemMetadata()
        childMetadata.ocId = "item-1"
        childMetadata.account = "TestAccount"
        childMetadata.serverUrl = "https://cloud.example.com/files/documents/projects"
        childMetadata.fileName = "report.pdf"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(rootDirectoryMetadata)
            realm.add(nestedDirectoryMetadata)
            realm.add(childMetadata)
        }

        let updatedChildren = Self.dbManager.renameDirectoryAndPropagateToChildren(
            ocId: "dir-1",
            newServerUrl: "https://cloud.example.com/storage",
            newFileName: "files"
        )

        XCTAssertNotNil(updatedChildren, "Should return updated children metadatas")
        XCTAssertEqual(updatedChildren?.count, 2, "Should include the nested directory and child item")
        XCTAssertTrue(
            updatedChildren?.contains { $0.serverUrl.contains("/storage/files/") } ?? false,
            "Should update serverUrl of all child items to reflect new directory path"
        )
    }

    func testDeletingDirectoryWithNoChildren() throws {
        let directoryMetadata = RealmItemMetadata()
        directoryMetadata.ocId = "dir-1"
        directoryMetadata.account = "TestAccount"
        directoryMetadata.serverUrl = "https://cloud.example.com/files"
        directoryMetadata.fileName = "empty"
        directoryMetadata.directory = true

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(directoryMetadata)
        }

        let deletedMetadatas = Self.dbManager.deleteDirectoryAndSubdirectoriesMetadata(
            ocId: "dir-1"
        )
        XCTAssertNotNil(
            deletedMetadatas,
            "Should return a list of deleted metadatas even if the directory has no children"
        )
        XCTAssertEqual(
            deletedMetadatas?.count,
            1,
            "Should only delete the directory itself as there are no children"
        )
    }

    func testRenamingDirectoryWithComplexNestedStructure() throws {
        // Create a complex nested directory structure
        let rootDirectoryMetadata = RealmItemMetadata()
        rootDirectoryMetadata.ocId = "dir-1"
        rootDirectoryMetadata.account = "TestAccount"
        rootDirectoryMetadata.serverUrl = "https://cloud.example.com/files"
        rootDirectoryMetadata.fileName = "dir-1"
        rootDirectoryMetadata.directory = true

        let nestedDirectoryMetadata = RealmItemMetadata()
        nestedDirectoryMetadata.ocId = "dir-2"
        nestedDirectoryMetadata.account = "TestAccount"
        nestedDirectoryMetadata.serverUrl = "https://cloud.example.com/files/dir-1"
        nestedDirectoryMetadata.fileName = "dir-2"
        nestedDirectoryMetadata.directory = true

        let deepNestedDirectoryMetadata = RealmItemMetadata()
        deepNestedDirectoryMetadata.ocId = "dir-3"
        deepNestedDirectoryMetadata.account = "TestAccount"
        deepNestedDirectoryMetadata.serverUrl = "https://cloud.example.com/files/dir-1/dir-2"
        deepNestedDirectoryMetadata.fileName = "dir-3"
        deepNestedDirectoryMetadata.directory = true

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(rootDirectoryMetadata)
            realm.add(nestedDirectoryMetadata)
            realm.add(deepNestedDirectoryMetadata)
        }

        let updatedChildren = Self.dbManager.renameDirectoryAndPropagateToChildren(
            ocId: "dir-1",
            newServerUrl: "https://cloud.example.com/storage",
            newFileName: "archives"
        )

        XCTAssertNotNil(updatedChildren, "Should return updated children metadatas")
        XCTAssertEqual(updatedChildren?.count, 2, "Should include both nested directories")
        XCTAssertTrue(
            updatedChildren?.allSatisfy { $0.serverUrl.contains("/storage/archives") } ?? false,
            "All children should have their serverUrl updated correctly"
        )
    }

    func testFindingItemBasedOnRemotePath() throws {
        let account = "TestAccount"
        let filename = "super duper new file"
        let parentUrl = "https://cloud.example.com/files/my great and incredible dir/dir-2"
        let fullUrl = parentUrl + "/" + filename

        let deepNestedDirectoryMetadata = RealmItemMetadata()
        deepNestedDirectoryMetadata.ocId = filename
        deepNestedDirectoryMetadata.account = account
        deepNestedDirectoryMetadata.serverUrl = parentUrl
        deepNestedDirectoryMetadata.fileName = filename
        deepNestedDirectoryMetadata.directory = true

        let realm = Self.dbManager.ncDatabase()
        try realm.write { realm.add(deepNestedDirectoryMetadata) }

        XCTAssertNotNil(Self.dbManager.itemMetadata(account: account, locatedAtRemoteUrl: fullUrl))
    }

    func testFindingItemBasedOnRemotePathInDirectoryWithHashtagInName() throws {
        let account = "TestAccount"
        let filename = "super duper new file"
        let parentUrl = "https://cloud.example.com/files/my great and # dir/dir-2"
        let fullUrl = parentUrl + "/" + filename

        let deepNestedDirectoryMetadata = RealmItemMetadata()
        deepNestedDirectoryMetadata.ocId = filename
        deepNestedDirectoryMetadata.account = account
        deepNestedDirectoryMetadata.serverUrl = parentUrl
        deepNestedDirectoryMetadata.fileName = filename
        deepNestedDirectoryMetadata.directory = true

        let realm = Self.dbManager.ncDatabase()
        try realm.write { realm.add(deepNestedDirectoryMetadata) }

        XCTAssertNotNil(Self.dbManager.itemMetadata(account: account, locatedAtRemoteUrl: fullUrl))
    }

    func testFindingItemBasedOnRemotePathInDirectoryWithQuestionMarkInName() throws {
        let account = "TestAccount"
        let filename = "super duper new file"
        let parentUrl = "https://cloud.example.com/files/my great and incredible dir ?/dir-2"
        let fullUrl = parentUrl + "/" + filename

        let deepNestedDirectoryMetadata = RealmItemMetadata()
        deepNestedDirectoryMetadata.ocId = filename
        deepNestedDirectoryMetadata.account = account
        deepNestedDirectoryMetadata.serverUrl = parentUrl
        deepNestedDirectoryMetadata.fileName = filename
        deepNestedDirectoryMetadata.directory = true

        let realm = Self.dbManager.ncDatabase()
        try realm.write { realm.add(deepNestedDirectoryMetadata) }

        XCTAssertNotNil(Self.dbManager.itemMetadata(account: account, locatedAtRemoteUrl: fullUrl))
    }

    func testFindingItemBasedOnCanonicallyEquivalentRemotePath() {
        let account = "TestAccount"
        let parentUrl = "https://cloud.example.com/files"
        let nfcFileName = "pr\u{00EA}t.pdf"
        let nfdFileName = "pre\u{0302}t.pdf"
        var metadata = SendableItemMetadata(ocId: "nfc-item", fileName: nfcFileName, account: Self.account)
        metadata.account = account
        metadata.serverUrl = parentUrl

        Self.dbManager.addItemMetadata(metadata)

        let result = Self.dbManager.itemMetadata(
            account: account,
            locatedAtRemoteUrl: parentUrl + "/" + nfdFileName
        )

        XCTAssertEqual(result?.ocId, metadata.ocId)
        XCTAssertEqual(result?.fileName, nfcFileName)
    }

    func testCanonicalPathDeduplicationPreservesIncomingRemoteSpelling() {
        let account = "TestAccount"
        let parentUrl = "https://cloud.example.com/files"
        let nfcFileName = "pr\u{00EA}t.pdf"
        let nfdFileName = "pre\u{0302}t.pdf"
        var existing = SendableItemMetadata(ocId: "nfc-item", fileName: nfcFileName, account: Self.account)
        existing.account = account
        existing.serverUrl = parentUrl
        existing.syncTime = Date(timeIntervalSince1970: 1)
        Self.dbManager.addItemMetadata(existing)

        var incoming = existing
        incoming.ocId = "nfd-item"
        incoming.fileName = nfdFileName
        incoming.fileNameView = nfdFileName
        incoming.syncTime = Date(timeIntervalSince1970: 2)
        Self.dbManager.addItemMetadata(incoming)

        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: "nfc-item")?.deleted, true)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: "nfd-item")?.fileName, nfdFileName)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: "nfd-item")?.remotePath(), parentUrl + "/" + nfdFileName)
    }

    func testKeepDownloadedSetting() throws {
        let existingMetadata = RealmItemMetadata()
        existingMetadata.ocId = "id-1"
        existingMetadata.fileName = "File.pdf"
        existingMetadata.account = "TestAccount"
        existingMetadata.serverUrl = "https://example.com"
        XCTAssertFalse(existingMetadata.keepDownloaded)

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(existingMetadata)
        }

        let sendable = SendableItemMetadata(value: existingMetadata)
        var updatedMetadata =
            try XCTUnwrap(Self.dbManager.set(keepDownloaded: true, for: sendable))
        XCTAssertTrue(updatedMetadata.keepDownloaded)

        updatedMetadata.keepDownloaded = false
        let finalMetadata =
            try XCTUnwrap(Self.dbManager.set(keepDownloaded: false, for: updatedMetadata))
        XCTAssertFalse(finalMetadata.keepDownloaded)
    }

    func testKeepDownloadedRetainedDuringDepth1ReadUpdate() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Create initial metadata with keepDownloaded = true
        var initialMetadata = SendableItemMetadata(ocId: "test-keep-downloaded", fileName: "test.txt", account: account)
        initialMetadata.downloaded = true
        initialMetadata.uploaded = true
        initialMetadata.keepDownloaded = true
        initialMetadata.etag = "old-etag"

        Self.dbManager.addItemMetadata(initialMetadata)

        // Verify initial state
        let storedMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "test-keep-downloaded"))
        XCTAssertTrue(storedMetadata.keepDownloaded, "Initial keepDownloaded should be true")
        XCTAssertTrue(storedMetadata.downloaded, "Initial downloaded should be true")

        // Update metadata with new etag (simulating server update)
        var updatedMetadata = initialMetadata
        updatedMetadata.etag = "new-etag"
        updatedMetadata.keepDownloaded = false // This would be the case when converting from NKFile

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [updatedMetadata],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result?.created.isEmpty, true, "Should create no new metadatas")
        XCTAssertEqual(result?.updated.isEmpty, false, "Should update existing metadata")

        // Verify keepDownloaded is retained
        let finalMetadata = try XCTUnwrap(result?.updated.first)
        XCTAssertTrue(finalMetadata.keepDownloaded, "keepDownloaded should be retained during update")
        XCTAssertTrue(finalMetadata.downloaded, "downloaded should be retained during update")
        XCTAssertEqual(finalMetadata.etag, "new-etag", "etag should be updated")

        // Verify in database
        let dbMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "test-keep-downloaded"))
        XCTAssertTrue(dbMetadata.keepDownloaded, "keepDownloaded should be retained in database")
    }

    func testKeepDownloadedRetainedWithKeepExistingDownloadStateFalse() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Create initial metadata with keepDownloaded = true
        var initialMetadata = SendableItemMetadata(ocId: "test-keep-downloaded-false", fileName: "test.txt", account: account)
        initialMetadata.downloaded = true
        initialMetadata.uploaded = true
        initialMetadata.keepDownloaded = true
        initialMetadata.etag = "old-etag"

        Self.dbManager.addItemMetadata(initialMetadata)

        // Update metadata with new etag
        var updatedMetadata = initialMetadata
        updatedMetadata.etag = "new-etag"
        updatedMetadata.keepDownloaded = false // This would be the case when converting from NKFile
        updatedMetadata.downloaded = false // Set to false to test keepDownloaded retention

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [updatedMetadata],
            keepExistingDownloadState: false // Even when not keeping download state
        )

        XCTAssertEqual(result?.updated.isEmpty, false, "Should update existing metadata")

        // Verify keepDownloaded is still retained even when keepExistingDownloadState is false
        let finalMetadata = try XCTUnwrap(result?.updated.first)
        XCTAssertTrue(finalMetadata.keepDownloaded, "keepDownloaded should be retained regardless of keepExistingDownloadState")
        XCTAssertEqual(finalMetadata.downloaded, false, "downloaded should not be retained when keepExistingDownloadState is false")
    }

    func testKeepDownloadedRetainedInReadTargetMetadata() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Create existing metadata with keepDownloaded = true
        var existingMetadata = SendableItemMetadata(ocId: "read-target-test", fileName: "target.txt", account: account)
        existingMetadata.keepDownloaded = true
        existingMetadata.downloaded = true
        existingMetadata.status = Status.normal.rawValue

        Self.dbManager.addItemMetadata(existingMetadata)

        // Create new read target metadata (simulating reading from server)
        var readTargetMetadata = SendableItemMetadata(ocId: "read-target-test", fileName: "target.txt", account: account)
        readTargetMetadata.etag = "new-etag"
        readTargetMetadata.keepDownloaded = false // Would be false when created from NKFile
        readTargetMetadata.downloaded = false

        // This simulates the path in depth1ReadUpdateItemMetadatas where readTargetMetadata
        // is processed and existing properties should be retained
        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [readTargetMetadata],
            keepExistingDownloadState: true
        )

        let updatedMetadata = try XCTUnwrap(result?.updated.first)
        XCTAssertTrue(updatedMetadata.keepDownloaded, "keepDownloaded should be retained in read target metadata")
        XCTAssertTrue(updatedMetadata.downloaded, "downloaded should be retained when keepExistingDownloadState is true")
    }

    func testKeepDownloadedNotInheritedWhenNoPinnedAncestor() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Create completely new metadata (not existing in database) with no
        // pinned ancestor: neither a parent row nor a pinned root container.
        var newMetadata = SendableItemMetadata(ocId: "new-item", fileName: "new.txt", account: account)
        newMetadata.etag = "initial-etag"
        newMetadata.keepDownloaded = false

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [newMetadata],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result?.created.isEmpty, false, "Should create new metadata")
        XCTAssertEqual(result?.updated.isEmpty, true, "Should not update any metadata")

        let createdMetadata = try XCTUnwrap(result?.created.first)
        XCTAssertFalse(createdMetadata.keepDownloaded, "keepDownloaded should remain false when no pinned ancestor exists")

        // Verify in database
        let dbMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "new-item"))
        XCTAssertFalse(dbMetadata.keepDownloaded, "keepDownloaded should be false in database when no pinned ancestor exists")
    }

    func testKeepDownloadedInheritedFromPinnedParentForNewMetadata() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Parent folder under the user's home, pinned.
        var parentFolder = SendableItemMetadata(ocId: "pinned-parent", fileName: "pinned", account: account)
        parentFolder.directory = true
        parentFolder.uploaded = true
        parentFolder.keepDownloaded = true
        parentFolder.etag = "parent-etag"

        Self.dbManager.addItemMetadata(parentFolder)

        // New child appearing remotely inside the pinned folder. Its
        // `serverUrl` is the parent's full path. `keepDownloaded` starts
        // `false`, mirroring what `NKFile.toItemMetadata()` produces.
        let parentRemotePath = account.davFilesUrl + "/pinned"
        var newChild = SendableItemMetadata(ocId: "fresh-child", fileName: "child.txt", account: account)
        newChild.serverUrl = parentRemotePath
        newChild.etag = "child-etag"
        newChild.keepDownloaded = false

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: parentRemotePath,
            updatedMetadatas: [parentFolder, newChild],
            keepExistingDownloadState: true
        )

        let createdChild = try XCTUnwrap(result?.created.first(where: { $0.ocId == "fresh-child" }))
        XCTAssertTrue(createdChild.keepDownloaded, "keepDownloaded should be inherited from pinned parent for new items")

        let dbChild = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "fresh-child"))
        XCTAssertTrue(dbChild.keepDownloaded, "Inherited keepDownloaded should be persisted to the database")
    }

    func testAddItemMetadataPreservingLocalState_InheritsPinnedParentForNewItem() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Pinned parent folder.
        var parentFolder = SendableItemMetadata(ocId: "preserving-parent", fileName: "pinned", account: account)
        parentFolder.directory = true
        parentFolder.uploaded = true
        parentFolder.keepDownloaded = true

        Self.dbManager.addItemMetadata(parentFolder)

        // Fresh child inside the pinned folder, exactly as it would arrive
        // from `NKFile.toItemMetadata()` (keepDownloaded = false).
        var freshChild = SendableItemMetadata(ocId: "preserving-child", fileName: "child.txt", account: account)
        freshChild.serverUrl = account.davFilesUrl + "/pinned"
        freshChild.keepDownloaded = false

        let written = Self.dbManager.addItemMetadataPreservingLocalState(freshChild)

        XCTAssertTrue(written.keepDownloaded, "keepDownloaded should be inherited from pinned parent on first insert")

        let dbChild = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "preserving-child"))
        XCTAssertTrue(dbChild.keepDownloaded, "Inherited keepDownloaded should be persisted to the database")
    }

    func testAddItemMetadataPreservingLocalState_DoesNotInheritFromUnpinnedParent() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Unpinned parent folder.
        var parentFolder = SendableItemMetadata(ocId: "unpinned-parent", fileName: "regular", account: account)
        parentFolder.directory = true
        parentFolder.uploaded = true
        parentFolder.keepDownloaded = false

        Self.dbManager.addItemMetadata(parentFolder)

        var freshChild = SendableItemMetadata(ocId: "unpinned-child", fileName: "child.txt", account: account)
        freshChild.serverUrl = account.davFilesUrl + "/regular"
        freshChild.keepDownloaded = false

        let written = Self.dbManager.addItemMetadataPreservingLocalState(freshChild)

        XCTAssertFalse(written.keepDownloaded, "keepDownloaded should remain false when parent is not pinned")

        let dbChild = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "unpinned-child"))
        XCTAssertFalse(dbChild.keepDownloaded, "Unpinned parent must not flip the child's keepDownloaded in the database")
    }

    func testKeepDownloadedRetainedWithMultipleItems() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        var parentFolder = SendableItemMetadata(ocId: "pf", fileName: "pf", account: account)
        parentFolder.uploaded = true
        parentFolder.etag = "old-pf"

        // Create multiple items with different keepDownloaded states
        var item1 = SendableItemMetadata(ocId: "multi-1", fileName: "file1.txt", account: account)
        item1.keepDownloaded = true
        item1.downloaded = true
        item1.uploaded = true
        item1.etag = "old-1"
        item1.serverUrl = account.davFilesUrl.appending("/pf")

        var item2 = SendableItemMetadata(ocId: "multi-2", fileName: "file2.txt", account: account)
        item2.keepDownloaded = false
        item2.downloaded = false
        item2.uploaded = true
        item2.etag = "old-2"
        item2.serverUrl = account.davFilesUrl.appending("/pf")

        var item3 = SendableItemMetadata(ocId: "multi-3", fileName: "file3.txt", account: account)
        item3.keepDownloaded = true
        item3.downloaded = false
        item3.uploaded = true
        item3.etag = "old-3"
        item3.serverUrl = account.davFilesUrl.appending("/pf")

        Self.dbManager.addItemMetadata(parentFolder)
        Self.dbManager.addItemMetadata(item1)
        Self.dbManager.addItemMetadata(item2)
        Self.dbManager.addItemMetadata(item3)

        // Update all items with new etags
        var updatedParentFolder = parentFolder
        updatedParentFolder.etag = "new-pf"

        var updatedItem1 = item1
        updatedItem1.etag = "new-1"
        updatedItem1.keepDownloaded = false // Would be reset when converting from NKFile

        var updatedItem2 = item2
        updatedItem2.etag = "new-2"
        updatedItem2.keepDownloaded = false

        var updatedItem3 = item3
        updatedItem3.etag = "new-3"
        updatedItem3.keepDownloaded = false

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl.appending("/pf"),
            updatedMetadatas: [updatedParentFolder, updatedItem1, updatedItem2, updatedItem3],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result?.updated.count, 4, "Should update all four items")

        // Verify each item's keepDownloaded state is correctly retained
        let updatedMetadatas = try XCTUnwrap(result?.updated)

        let finalItem1 = try XCTUnwrap(updatedMetadatas.first { $0.ocId == "multi-1" })
        XCTAssertTrue(finalItem1.keepDownloaded, "Item 1 should retain keepDownloaded = true")

        let finalItem2 = try XCTUnwrap(updatedMetadatas.first { $0.ocId == "multi-2" })
        XCTAssertFalse(finalItem2.keepDownloaded, "Item 2 should retain keepDownloaded = false")

        let finalItem3 = try XCTUnwrap(updatedMetadatas.first { $0.ocId == "multi-3" })
        XCTAssertTrue(finalItem3.keepDownloaded, "Item 3 should retain keepDownloaded = true")
    }

    func testParentItemIdentifierWithRemoteFallback() async throws {
        let rootItem = MockRemoteItem.rootItem(account: Self.account)

        let remoteFolder = MockRemoteItem(
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

        let remoteItem = MockRemoteItem(
            identifier: "item",
            versionIdentifier: "NEW",
            name: "item",
            remotePath: Self.account.davFilesUrl + "/folder/item",
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        rootItem.children = [remoteFolder]
        remoteFolder.parent = rootItem
        remoteFolder.children = [remoteItem]
        remoteItem.parent = remoteFolder

        let remoteItemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(remoteItemMetadata)
        XCTAssertNil(Self.dbManager.parentItemIdentifierFromMetadata(remoteItemMetadata))

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.injectMock(Self.account)

        let retrievedParentIdentifier = await Self.dbManager.parentItemIdentifierWithRemoteFallback(
            fromMetadata: remoteItemMetadata,
            remoteInterface: remoteInterface,
            account: Self.account
        )

        let unwrappedParentIdentifier = try XCTUnwrap(retrievedParentIdentifier)
        XCTAssertEqual(unwrappedParentIdentifier.rawValue, remoteFolder.identifier)
    }

    func testMaterialisedFiles() throws {
        let itemA = RealmItemMetadata()
        let itemB = RealmItemMetadata()
        let folderA = RealmItemMetadata()
        let folderB = RealmItemMetadata()
        let notFolderA = RealmItemMetadata()

        folderA.directory = true
        folderB.directory = true

        itemA.ocId = "itemA"
        itemB.ocId = "itemB"
        folderA.ocId = "folderA"
        folderB.ocId = "folderB"
        notFolderA.ocId = "notFolderA"

        itemA.account = Self.account.ncKitAccount
        itemB.account = Self.account.ncKitAccount
        folderA.account = Self.account.ncKitAccount
        folderB.account = Self.account.ncKitAccount
        notFolderA.account = Self.account.ncKitAccount

        itemA.downloaded = true
        itemB.downloaded = false
        folderA.visitedDirectory = true
        folderB.visitedDirectory = false
        notFolderA.visitedDirectory = true

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(itemA)
            realm.add(itemB)
            realm.add(folderA)
            realm.add(folderB)
        }

        // Test with addItemMetadata too
        var sItemA = SendableItemMetadata(ocId: "sItemA", fileName: "sItemA", account: Self.account)
        sItemA.downloaded = true

        var sItemB = SendableItemMetadata(ocId: "sItemB", fileName: "sItemB", account: Self.account)
        sItemB.downloaded = false

        var sItemC = SendableItemMetadata(ocId: "sItemC", fileName: "sItemC", account: Self.account)
        sItemC.downloaded = true

        var sDirD = SendableItemMetadata(ocId: "sDirD", fileName: "sDirD", account: Self.account)
        sDirD.directory = true
        sDirD.visitedDirectory = true

        Self.dbManager.addItemMetadata(sItemA)
        Self.dbManager.addItemMetadata(sItemB)
        Self.dbManager.addItemMetadata(sItemC)
        Self.dbManager.addItemMetadata(sDirD)

        let materialized =
            Self.dbManager.materialisedItemMetadatas(account: Self.account.ncKitAccount)
        XCTAssertEqual(materialized.count, 5)

        let materialisedOcIds = materialized.map(\.ocId)
        XCTAssertTrue(materialisedOcIds.contains(itemA.ocId))
        XCTAssertTrue(materialisedOcIds.contains(folderA.ocId))
        XCTAssertTrue(materialisedOcIds.contains(sItemA.ocId))
        XCTAssertTrue(materialisedOcIds.contains(sItemC.ocId))
        XCTAssertTrue(materialisedOcIds.contains(sDirD.ocId))
    }

    func testKeepDownloadedRetainedDuringUpdate() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // Create initial metadata with keepDownloaded = true
        var initialMetadata = SendableItemMetadata(ocId: "test-keep-downloaded", fileName: "test.txt", account: account)
        initialMetadata.downloaded = true
        initialMetadata.uploaded = true
        initialMetadata.keepDownloaded = true
        initialMetadata.etag = "old-etag"

        Self.dbManager.addItemMetadata(initialMetadata)

        // Verify initial state
        let storedMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "test-keep-downloaded"))
        XCTAssertTrue(storedMetadata.keepDownloaded, "Initial keepDownloaded should be true")
        XCTAssertTrue(storedMetadata.downloaded, "Initial downloaded should be true")

        // Update metadata with new etag (simulating server update)
        var updatedMetadata = initialMetadata
        updatedMetadata.etag = "new-etag"
        updatedMetadata.keepDownloaded = false // This would be the case when converting from NKFile

        let result = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [updatedMetadata],
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result?.created.isEmpty, true, "Should create no new metadatas")
        XCTAssertEqual(result?.updated.isEmpty, false, "Should update existing metadata")

        // Verify keepDownloaded is retained
        let finalMetadata = try XCTUnwrap(result?.updated.first)
        XCTAssertTrue(finalMetadata.keepDownloaded, "keepDownloaded should be retained during update")
        XCTAssertTrue(finalMetadata.downloaded, "downloaded should be retained during update")
        XCTAssertEqual(finalMetadata.etag, "new-etag", "etag should be updated")

        // Verify in database
        let dbMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "test-keep-downloaded"))
        XCTAssertTrue(dbMetadata.keepDownloaded, "keepDownloaded should be retained in database")
    }

    func testPendingWorkingSetChanges() {
        // 1. Arrange
        let anchorDate = Date().addingTimeInterval(-300) // 5 minutes ago
        let oldSyncDate = Date().addingTimeInterval(-600) // 10 minutes ago (before anchor)
        let recentSyncDate = Date() // Now (after anchor)

        // --- Multi-level file structure to test the full scope ---

        // LEVEL 1: Root level materialized folder updated recently
        var updatedDir = SendableItemMetadata(ocId: "updatedDir", fileName: "UpdatedDir", account: Self.account)
        updatedDir.directory = true
        updatedDir.visitedDirectory = true // Materialised
        updatedDir.syncTime = recentSyncDate
        Self.dbManager.addItemMetadata(updatedDir)

        // LEVEL 2: Child of updated folder with OLD sync time - should NOT be included
        var childOfUpdatedDirOld = SendableItemMetadata(ocId: "childOfUpdatedOld", fileName: "childOld.txt", account: Self.account)
        childOfUpdatedDirOld.serverUrl = Self.account.davFilesUrl + "/UpdatedDir"
        childOfUpdatedDirOld.syncTime = oldSyncDate // Old sync time
        childOfUpdatedDirOld.downloaded = true // Materialised
        Self.dbManager.addItemMetadata(childOfUpdatedDirOld)

        // LEVEL 2: Child of updated folder with RECENT sync time - should be included (regardless of materialisation)
        var childOfUpdatedDirRecent = SendableItemMetadata(ocId: "childOfUpdatedRecent", fileName: "childRecent.txt", account: Self.account)
        childOfUpdatedDirRecent.serverUrl = Self.account.davFilesUrl + "/UpdatedDir"
        childOfUpdatedDirRecent.syncTime = recentSyncDate // Recent sync time
        childOfUpdatedDirRecent.downloaded = false // NOT materialized - but should still be included
        Self.dbManager.addItemMetadata(childOfUpdatedDirRecent)

        // LEVEL 2: Child folder of updated folder with recent sync time
        var childFolderOfUpdated = SendableItemMetadata(ocId: "childFolderOfUpdated", fileName: "ChildFolder", account: Self.account)
        childFolderOfUpdated.directory = true
        childFolderOfUpdated.visitedDirectory = false // Not materialised
        childFolderOfUpdated.serverUrl = Self.account.davFilesUrl + "/UpdatedDir"
        childFolderOfUpdated.syncTime = recentSyncDate
        Self.dbManager.addItemMetadata(childFolderOfUpdated)

        // LEVEL 3: Grandchild with recent sync time - should not be included
        var grandchildOfUpdated = SendableItemMetadata(ocId: "grandchildOfUpdated", fileName: "grandchild.txt", account: Self.account)
        grandchildOfUpdated.serverUrl = Self.account.davFilesUrl + "/UpdatedDir/ChildFolder"
        grandchildOfUpdated.syncTime = recentSyncDate
        grandchildOfUpdated.downloaded = false // Not materialised
        Self.dbManager.addItemMetadata(grandchildOfUpdated)

        // DELETED STRUCTURE: Root level materialized folder deleted recently
        var deletedDir = SendableItemMetadata(ocId: "deletedDir", fileName: "DeletedDir", account: Self.account)
        deletedDir.directory = true
        deletedDir.visitedDirectory = true // Materialised
        deletedDir.deleted = true
        deletedDir.syncTime = recentSyncDate
        Self.dbManager.addItemMetadata(deletedDir)

        // Child of deleted folder with OLD sync time - should NOT be included
        var childOfDeletedDirOld = SendableItemMetadata(ocId: "childOfDeletedOld", fileName: "childDelOld.txt", account: Self.account)
        childOfDeletedDirOld.serverUrl = Self.account.davFilesUrl + "/DeletedDir"
        childOfDeletedDirOld.syncTime = oldSyncDate // Old sync time
        childOfDeletedDirOld.downloaded = true
        Self.dbManager.addItemMetadata(childOfDeletedDirOld)

        // Child of deleted folder with RECENT sync time - should be included in deleted
        var childOfDeletedDirRecent = SendableItemMetadata(ocId: "childOfDeletedRecent", fileName: "childDelRecent.txt", account: Self.account)
        childOfDeletedDirRecent.serverUrl = Self.account.davFilesUrl + "/DeletedDir"
        childOfDeletedDirRecent.syncTime = recentSyncDate
        childOfDeletedDirRecent.downloaded = false // Not materialised
        Self.dbManager.addItemMetadata(childOfDeletedDirRecent)

        // Nested structure under deleted folder
        var nestedFolderInDeleted = SendableItemMetadata(ocId: "nestedFolderInDeleted", fileName: "NestedFolder", account: Self.account)
        nestedFolderInDeleted.directory = true
        nestedFolderInDeleted.visitedDirectory = false
        nestedFolderInDeleted.serverUrl = Self.account.davFilesUrl + "/DeletedDir"
        nestedFolderInDeleted.syncTime = recentSyncDate
        Self.dbManager.addItemMetadata(nestedFolderInDeleted)

        // Deep nested item under deleted structure
        var deepNestedInDeleted = SendableItemMetadata(ocId: "deepNestedInDeleted", fileName: "deepNested.txt", account: Self.account)
        deepNestedInDeleted.serverUrl = Self.account.davFilesUrl + "/DeletedDir/NestedFolder"
        deepNestedInDeleted.syncTime = recentSyncDate
        deepNestedInDeleted.downloaded = false
        Self.dbManager.addItemMetadata(deepNestedInDeleted)

        // STANDALONE ITEMS: materialized file synced recently - should be returned
        var standaloneUpdatedFile = SendableItemMetadata(ocId: "standaloneUpdated", fileName: "standalone.txt", account: Self.account)
        standaloneUpdatedFile.downloaded = true // Materialised
        standaloneUpdatedFile.syncTime = recentSyncDate
        Self.dbManager.addItemMetadata(standaloneUpdatedFile)

        // materialized file synced too long ago - should NOT be returned
        var standaloneOldFile = SendableItemMetadata(ocId: "standaloneOld", fileName: "old.txt", account: Self.account)
        standaloneOldFile.downloaded = true // Materialised
        standaloneOldFile.syncTime = oldSyncDate
        Self.dbManager.addItemMetadata(standaloneOldFile)

        // Non-materialised item synced recently - should NOT be returned (not in initial materialized set)
        var nonMaterialisedFile = SendableItemMetadata(ocId: "nonMaterialised", fileName: "non-mat.txt", account: Self.account)
        nonMaterialisedFile.downloaded = false
        nonMaterialisedFile.syncTime = recentSyncDate
        Self.dbManager.addItemMetadata(nonMaterialisedFile)

        // MIXED MATERIALISATION: Another materialized folder to test child inclusion
        var anotherMaterialisedDir = SendableItemMetadata(ocId: "anotherMatDir", fileName: "AnotherMatDir", account: Self.account)
        anotherMaterialisedDir.directory = true
        anotherMaterialisedDir.visitedDirectory = true
        anotherMaterialisedDir.syncTime = recentSyncDate
        Self.dbManager.addItemMetadata(anotherMaterialisedDir)

        // Child with recent sync but NOT materialized - should still be included due to recent sync
        var nonMatChildRecent = SendableItemMetadata(ocId: "nonMatChildRecent", fileName: "nonMatChild.txt", account: Self.account)
        nonMatChildRecent.serverUrl = Self.account.davFilesUrl + "/AnotherMatDir"
        nonMatChildRecent.syncTime = recentSyncDate
        nonMatChildRecent.downloaded = false // Not materialised
        Self.dbManager.addItemMetadata(nonMatChildRecent)

        // 2. Act
        let result = Self.dbManager.pendingWorkingSetChanges(since: anchorDate)

        // 3. Assert - Updated items
        let updatedIds = Set(result.updated.map(\.ocId))

        // Should include materialized items with recent sync
        XCTAssertTrue(updatedIds.contains("updatedDir"), "Updated materialized directory should be included")
        XCTAssertTrue(updatedIds.contains("standaloneUpdated"), "Updated materialized file should be included")
        XCTAssertTrue(updatedIds.contains("anotherMatDir"), "Another materialized directory should be included")

        // Should include children with recent sync regardless of materialisation
        XCTAssertTrue(updatedIds.contains("childOfUpdatedRecent"), "Child with recent sync should be included regardless of materialisation")
        XCTAssertTrue(updatedIds.contains("childFolderOfUpdated"), "Child folder with recent sync should be included")
        XCTAssertTrue(updatedIds.contains("nonMatChildRecent"), "Non-materialised child with recent sync should be included")

        // Should NOT include items with old sync times
        XCTAssertFalse(updatedIds.contains("childOfUpdatedOld"), "Child with old sync time should NOT be included")
        XCTAssertFalse(updatedIds.contains("standaloneOld"), "Materialised file with old sync should NOT be included")

        // Should NOT include non-materialised items not under a recently updated path
        XCTAssertFalse(updatedIds.contains("nonMaterialised"), "Standalone non-materialised item should NOT be included")

        // 4. Assert - Deleted items
        let deletedIds = Set(result.deleted.map(\.ocId))

        // Should include the deleted materialized directory
        XCTAssertTrue(deletedIds.contains("deletedDir"), "Deleted materialized directory should be included")

        // Should include children/descendants with recent sync under deleted paths
        XCTAssertTrue(deletedIds.contains("childOfDeletedRecent"), "Child of deleted dir with recent sync should be included")
        XCTAssertTrue(deletedIds.contains("nestedFolderInDeleted"), "Nested folder under deleted dir should be included")
        XCTAssertTrue(deletedIds.contains("deepNestedInDeleted"), "Deep nested item under deleted structure should be included")

        // Should NOT include children with old sync times
        XCTAssertFalse(deletedIds.contains("childOfDeletedOld"), "Child of deleted dir with old sync should NOT be included")

        // 5. Verify expected counts
        let expectedUpdatedCount = 6 // updatedDir, standaloneUpdated, anotherMatDir, childOfUpdatedRecent, childFolderOfUpdated, nonMatChildRecent
        let expectedDeletedCount = 4 // deletedDir, childOfDeletedRecent, nestedFolderInDeleted, deepNestedInDeleted

        XCTAssertEqual(updatedIds.count, expectedUpdatedCount, "Should have \(expectedUpdatedCount) updated items, got \(updatedIds.count): \(updatedIds)")
        XCTAssertEqual(deletedIds.count, expectedDeletedCount, "Should have \(expectedDeletedCount) deleted items, got \(deletedIds.count): \(deletedIds)")
    }

    // MARK: - Logical-address deduplication

    func testAddItemMetadataEvictsLogicalDuplicateWithDifferentOcId() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        var rowA = SendableItemMetadata(ocId: "A", fileName: "shared.txt", account: account)
        rowA.syncTime = Date(timeIntervalSince1970: 1000)
        Self.dbManager.addItemMetadata(rowA)

        var rowB = SendableItemMetadata(ocId: "B", fileName: "shared.txt", account: account)
        rowB.syncTime = Date(timeIntervalSince1970: 2000)
        Self.dbManager.addItemMetadata(rowB)

        let storedA = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "A"))
        XCTAssertTrue(storedA.deleted, "Logical duplicate with different ocId should be soft-deleted")
        XCTAssertGreaterThan(storedA.syncTime, rowA.syncTime, "Eviction should bump syncTime")

        let storedB = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "B"))
        XCTAssertFalse(storedB.deleted, "Newly added row should not be deleted")
    }

    func testDeletedIncomingDoesNotEvictLiveLogicalSibling() throws {
        // Regression for ticket 96101301: an app "safe save" (create -> delete ->
        // recreate) rotates the ocId, leaving a stale tombstone beside the live,
        // server-confirmed row at the same logical path. Persisting that tombstone
        // must NOT soft-delete the live sibling, or the freshly recreated file
        // vanishes from Finder.
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        // The live, server-confirmed row for the recreated file.
        var live = SendableItemMetadata(ocId: "104", fileName: "TEST 1.pdf", account: account)
        live.syncTime = Date(timeIntervalSince1970: 1000)
        Self.dbManager.addItemMetadata(live)

        // The tombstone of the stale ocId at the same logical address, being
        // persisted by the working-set scan's deletion path with a bumped syncTime.
        var tombstone = SendableItemMetadata(ocId: "99", fileName: "TEST 1.pdf", account: account)
        tombstone.deleted = true
        tombstone.syncTime = Date(timeIntervalSince1970: 2000)
        Self.dbManager.addItemMetadata(tombstone)

        let storedLive = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "104"))
        XCTAssertFalse(storedLive.deleted, "A tombstone incoming must not soft-delete the live sibling")

        let storedTombstone = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "99"))
        XCTAssertTrue(storedTombstone.deleted, "The tombstone itself should remain recorded as deleted")
    }

    func testAddItemMetadataSameOcIdSameLogicalPathIsNormalUpsert() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        var rowA = SendableItemMetadata(ocId: "A", fileName: "shared.txt", account: account)
        rowA.etag = "v1"
        Self.dbManager.addItemMetadata(rowA)

        rowA.etag = "v2"
        Self.dbManager.addItemMetadata(rowA)

        let stored = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "A"))
        XCTAssertFalse(stored.deleted, "Same-ocId upsert should not soft-delete the row")
        XCTAssertEqual(stored.etag, "v2", "etag should be updated by the upsert")
    }

    func testDepth1ReadEvictsStaleLogicalSiblingOnFreshOcId() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        var rootMetadata = SendableItemMetadata(ocId: "root", fileName: "", account: account)
        rootMetadata.directory = true
        rootMetadata.uploaded = true
        Self.dbManager.addItemMetadata(rootMetadata)

        // Seed an existing row at the target logical path with `uploaded == false`
        // so the depth-1-read delete-on-update path will not consider it; only
        // the new eviction code can remove it.
        let realm = Self.dbManager.ncDatabase()
        let oldRow = RealmItemMetadata()
        oldRow.ocId = "oldOcId"
        oldRow.account = account.ncKitAccount
        oldRow.serverUrl = account.davFilesUrl
        oldRow.fileName = "TOOLS and WORKFLOWS"
        oldRow.directory = true
        oldRow.uploaded = false
        oldRow.syncTime = Date(timeIntervalSince1970: 1000)
        try realm.write { realm.add(oldRow) }

        var freshDir = SendableItemMetadata(ocId: "freshOcId", fileName: "TOOLS and WORKFLOWS", account: account)
        freshDir.directory = true
        freshDir.uploaded = true

        _ = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [rootMetadata, freshDir],
            keepExistingDownloadState: true
        )

        let storedOld = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "oldOcId"))
        XCTAssertTrue(storedOld.deleted, "Stale logical sibling should be soft-deleted by eviction")

        let storedFresh = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "freshOcId"))
        XCTAssertFalse(storedFresh.deleted, "Fresh ocId row should be persisted")
    }

    func testDepth1ReadDoesNotEvictInFlightSibling() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        var rootMetadata = SendableItemMetadata(ocId: "root", fileName: "", account: account)
        rootMetadata.directory = true
        rootMetadata.uploaded = true
        Self.dbManager.addItemMetadata(rootMetadata)

        let realm = Self.dbManager.ncDatabase()
        let inFlightRow = RealmItemMetadata()
        inFlightRow.ocId = "uploading"
        inFlightRow.account = account.ncKitAccount
        inFlightRow.serverUrl = account.davFilesUrl
        inFlightRow.fileName = "upload-in-progress.bin"
        inFlightRow.uploaded = false
        inFlightRow.status = Status.uploading.rawValue
        try realm.write { realm.add(inFlightRow) }

        var fresh = SendableItemMetadata(ocId: "freshOcId", fileName: "upload-in-progress.bin", account: account)
        fresh.uploaded = true

        _ = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [rootMetadata, fresh],
            keepExistingDownloadState: true
        )

        let storedInFlight = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "uploading"))
        XCTAssertFalse(storedInFlight.deleted, "In-flight sibling should not be soft-deleted")

        let storedFresh = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "freshOcId"))
        XCTAssertFalse(storedFresh.deleted, "Fresh row should be inserted alongside the in-flight one")
    }

    func testStartupCleanupDeduplicatesSeededDuplicate() throws {
        let testAccount = "TestAccount"
        let testServerUrl = "https://example.com"
        let fileName = "dup.txt"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            let older = RealmItemMetadata()
            older.ocId = "older"
            older.account = testAccount
            older.serverUrl = testServerUrl
            older.fileName = fileName
            older.syncTime = Date(timeIntervalSince1970: 1000)
            realm.add(older)

            let newer = RealmItemMetadata()
            newer.ocId = "newer"
            newer.account = testAccount
            newer.serverUrl = testServerUrl
            newer.fileName = fileName
            newer.syncTime = Date(timeIntervalSince1970: 2000)
            realm.add(newer)
        }

        let configuration = Realm.Configuration.defaultConfiguration
        let manager = FilesDatabaseManager(
            realmConfiguration: configuration,
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )

        let storedOlder = try XCTUnwrap(manager.itemMetadata(ocId: "older"))
        XCTAssertTrue(storedOlder.deleted, "Older duplicate should be soft-deleted at startup")

        let storedNewer = try XCTUnwrap(manager.itemMetadata(ocId: "newer"))
        XCTAssertFalse(storedNewer.deleted, "Newer duplicate should be kept")
    }

    func testStartupCleanupTiebreakOnEqualSyncTime() throws {
        let testAccount = "TestAccount"
        let testServerUrl = "https://example.com"
        let fileName = "tie.txt"
        let sameTime = Date(timeIntervalSince1970: 5000)

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            let a = RealmItemMetadata()
            a.ocId = "aa"
            a.account = testAccount
            a.serverUrl = testServerUrl
            a.fileName = fileName
            a.syncTime = sameTime
            realm.add(a)

            let b = RealmItemMetadata()
            b.ocId = "bb"
            b.account = testAccount
            b.serverUrl = testServerUrl
            b.fileName = fileName
            b.syncTime = sameTime
            realm.add(b)
        }

        let configuration = Realm.Configuration.defaultConfiguration
        let manager = FilesDatabaseManager(
            realmConfiguration: configuration,
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )

        let storedA = try XCTUnwrap(manager.itemMetadata(ocId: "aa"))
        XCTAssertTrue(storedA.deleted, "Lexicographically smaller ocId loses the tiebreak")

        let storedB = try XCTUnwrap(manager.itemMetadata(ocId: "bb"))
        XCTAssertFalse(storedB.deleted, "Lexicographically greater ocId wins the tiebreak")
    }

    func testStartupCleanupNoOpWhenNoDuplicates() throws {
        let testAccount = "TestAccount"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            let a = RealmItemMetadata()
            a.ocId = "one"
            a.account = testAccount
            a.serverUrl = "https://example.com"
            a.fileName = "one.txt"
            realm.add(a)

            let b = RealmItemMetadata()
            b.ocId = "two"
            b.account = testAccount
            b.serverUrl = "https://example.com"
            b.fileName = "two.txt"
            realm.add(b)

            // Same fileName but different serverUrl — not a logical duplicate.
            let c = RealmItemMetadata()
            c.ocId = "three"
            c.account = testAccount
            c.serverUrl = "https://example.com/folder"
            c.fileName = "one.txt"
            realm.add(c)
        }

        let configuration = Realm.Configuration.defaultConfiguration
        let manager = FilesDatabaseManager(
            realmConfiguration: configuration,
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )

        XCTAssertFalse(try XCTUnwrap(manager.itemMetadata(ocId: "one")).deleted)
        XCTAssertFalse(try XCTUnwrap(manager.itemMetadata(ocId: "two")).deleted)
        XCTAssertFalse(try XCTUnwrap(manager.itemMetadata(ocId: "three")).deleted)
    }

    func testStartupCleanupSkipsInTransitDuplicate() throws {
        let testAccount = "TestAccount"
        let testServerUrl = "https://example.com"
        let fileName = "in-flight.bin"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            let uploading = RealmItemMetadata()
            uploading.ocId = "uploading"
            uploading.account = testAccount
            uploading.serverUrl = testServerUrl
            uploading.fileName = fileName
            uploading.status = Status.uploading.rawValue
            uploading.syncTime = Date(timeIntervalSince1970: 5000)
            realm.add(uploading)

            let settled = RealmItemMetadata()
            settled.ocId = "settled"
            settled.account = testAccount
            settled.serverUrl = testServerUrl
            settled.fileName = fileName
            settled.status = Status.normal.rawValue
            settled.syncTime = Date(timeIntervalSince1970: 1000)
            realm.add(settled)
        }

        let configuration = Realm.Configuration.defaultConfiguration
        let manager = FilesDatabaseManager(
            realmConfiguration: configuration,
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )

        let storedUploading = try XCTUnwrap(manager.itemMetadata(ocId: "uploading"))
        XCTAssertFalse(storedUploading.deleted, "In-flight row must remain intact")

        let storedSettled = try XCTUnwrap(manager.itemMetadata(ocId: "settled"))
        XCTAssertFalse(storedSettled.deleted, "Settled row remains because it is the only writable winner")
    }

    func testEvictionRespectsLockFileOfLocalOrigin() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")
        let fileName = "~$doc.docx"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            let localLock = RealmItemMetadata()
            localLock.ocId = "local-lock"
            localLock.account = account.ncKitAccount
            localLock.serverUrl = account.davFilesUrl
            localLock.fileName = fileName
            localLock.isLockFileOfLocalOrigin = true
            realm.add(localLock)
        }

        let serverRow = SendableItemMetadata(ocId: "server-row", fileName: fileName, account: account)
        Self.dbManager.addItemMetadata(serverRow)

        let storedLocalLock = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "local-lock"))
        XCTAssertFalse(storedLocalLock.deleted, "Lock file of local origin must not be evicted")

        let storedServer = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "server-row"))
        XCTAssertFalse(storedServer.deleted, "Server-side row is inserted alongside the lock file")
    }

    func testEvictedRowSurfacesInPendingWorkingSetChanges() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")
        let anchor = Date()

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            let preExisting = RealmItemMetadata()
            preExisting.ocId = "preExisting"
            preExisting.account = account.ncKitAccount
            preExisting.serverUrl = account.davFilesUrl
            preExisting.fileName = "materialized.txt"
            preExisting.downloaded = true // materialised
            preExisting.syncTime = anchor.addingTimeInterval(-3600)
            realm.add(preExisting)
        }

        var fresh = SendableItemMetadata(ocId: "fresh", fileName: "materialized.txt", account: account)
        fresh.downloaded = true
        fresh.syncTime = anchor.addingTimeInterval(60)
        Self.dbManager.addItemMetadata(fresh)

        let changes = Self.dbManager.pendingWorkingSetChanges(since: anchor)
        XCTAssertTrue(
            changes.deleted.contains { $0.ocId == "preExisting" },
            "Evicted materialized row should surface in working-set deletions"
        )
    }

    func testAddItemMetadataPreservingLocalStateFallsBackOnOcIdRotation() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

        var original = SendableItemMetadata(ocId: "originalOcId", fileName: "rotated.txt", account: account)
        original.keepDownloaded = true
        original.downloaded = true
        Self.dbManager.addItemMetadata(original)

        var rotated = SendableItemMetadata(ocId: "rotatedOcId", fileName: "rotated.txt", account: account)
        rotated.keepDownloaded = false // fresh from server, no local state
        rotated.downloaded = false

        let merged = Self.dbManager.addItemMetadataPreservingLocalState(rotated)

        XCTAssertTrue(merged.keepDownloaded, "Should carry over keepDownloaded across the ocId rotation")
        XCTAssertTrue(merged.downloaded, "Should carry over downloaded across the ocId rotation")

        let storedOriginal = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "originalOcId"))
        XCTAssertTrue(storedOriginal.deleted, "Original row should be soft-deleted after rotation")

        let storedRotated = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "rotatedOcId"))
        XCTAssertFalse(storedRotated.deleted)
        XCTAssertTrue(storedRotated.keepDownloaded, "Persisted row should reflect the merged state")
        XCTAssertTrue(storedRotated.downloaded)
    }

    func testAddItemMetadataPreservingLocalStateFallbackDoesNotMergeWhenAlreadyDuplicated() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")
        let fileName = "duped.txt"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            let a1 = RealmItemMetadata()
            a1.ocId = "A1"
            a1.account = account.ncKitAccount
            a1.serverUrl = account.davFilesUrl
            a1.fileName = fileName
            a1.keepDownloaded = true
            a1.downloaded = true
            a1.syncTime = Date(timeIntervalSince1970: 1000)
            realm.add(a1)

            let a2 = RealmItemMetadata()
            a2.ocId = "A2"
            a2.account = account.ncKitAccount
            a2.serverUrl = account.davFilesUrl
            a2.fileName = fileName
            a2.keepDownloaded = true
            a2.downloaded = true
            a2.syncTime = Date(timeIntervalSince1970: 2000)
            realm.add(a2)
        }

        var fresh = SendableItemMetadata(ocId: "B", fileName: fileName, account: account)
        fresh.keepDownloaded = false
        fresh.downloaded = false

        let merged = Self.dbManager.addItemMetadataPreservingLocalState(fresh)

        XCTAssertFalse(merged.keepDownloaded, "Should not merge state when the database is already duplicated")
        XCTAssertFalse(merged.downloaded, "Should not merge state when the database is already duplicated")

        let storedA1 = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "A1"))
        XCTAssertTrue(storedA1.deleted, "Pre-existing duplicate A1 should be soft-deleted by eviction")

        let storedA2 = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "A2"))
        XCTAssertTrue(storedA2.deleted, "Pre-existing duplicate A2 should be soft-deleted by eviction")

        let storedB = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: "B"))
        XCTAssertFalse(storedB.deleted)
    }
}
