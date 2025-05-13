//
//  FilesDatabaseManagerTests.swift
//
//
//  Created by Claudio Cambra on 15/5/24.
//

import FileProvider
import Foundation
import RealmSwift
import XCTest
@testable import NextcloudFileProviderKit

final class FilesDatabaseManagerTests: XCTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    static let dbManager = FilesDatabaseManager(
        realmConfig: .defaultConfiguration, account: account
    )

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    func testFilesDatabaseManagerInitialization() {
        XCTAssertNotNil(Self.dbManager, "FilesDatabaseManager should be initialized")
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
            updateDirectoryEtags: true,
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result.newMetadatas?.isEmpty, false, "Should create new metadatas")
        XCTAssertEqual(result.updatedMetadatas?.isEmpty, true, "No metadata should be updated")

        // Now test we are receiving the updated basic metadatas correctly.
        metadata.etag = "new and shiny"

        let result2 = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [metadata],
            updateDirectoryEtags: true,
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result2.newMetadatas?.isEmpty, true, "Should create no new metadatas")
        XCTAssertEqual(result2.updatedMetadatas?.isEmpty, false, "Metadata should be updated")

        // Also check the download state is correctly kept the same.
        // We set it to false here to replicate the lack of a download state when converting from
        // the NKFiles received during remote enumeration
        metadata.downloaded = false
        metadata.etag = "new and shiny, but keeping original download state"

        let result3 = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [metadata],
            updateDirectoryEtags: true,
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result3.newMetadatas?.isEmpty, true, "Should create no new metadatas")
        XCTAssertEqual(result3.updatedMetadatas?.isEmpty, false, "Metadata should be updated")
        XCTAssertEqual(result3.updatedMetadatas?.first?.downloaded, true)
    }

    func testUpdateRenamesDirectoryAndPropagatesToChildren() throws {
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")

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
            updatedMetadatas: [renamedParent],
            updateDirectoryEtags: true,
            keepExistingDownloadState: true
        )

        // Ensure rename took place
        XCTAssertEqual(result.newMetadatas?.isEmpty, true)
        XCTAssertEqual(result.updatedMetadatas?.isEmpty, false)
        XCTAssertNotNil(result.updatedMetadatas?.first(where: { $0.fileName == "newDir" }))

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
            updateDirectoryEtags: true,
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result.updatedMetadatas?.isEmpty, true, "Transit items should not be updated")
        XCTAssertEqual(result.newMetadatas?.isEmpty, true)
        XCTAssertEqual(result.deletedMetadatas?.isEmpty, true)

        let inDb = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: transit.ocId))
        XCTAssertEqual(inDb.etag, transit.etag)
    }

    func testTransitItemIsDeleted() throws {
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
            updateDirectoryEtags: true,
            keepExistingDownloadState: true
        )

        XCTAssertEqual(result.updatedMetadatas?.isEmpty, true)
        XCTAssertEqual(result.newMetadatas?.isEmpty, true)
        XCTAssertEqual(result.deletedMetadatas?.isEmpty, false)
        XCTAssertEqual(result.deletedMetadatas?.first?.ocId, transit.ocId)
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
        XCTAssertNil(
            Self.dbManager.itemMetadata(ocId: ocId),
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
        let updatedMetadatas = [existingMetadata1, existingMetadata3]  // Only include 2 of the 3

        let _ = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: "TestAccount",
            serverUrl: "https://example.com",
            updatedMetadatas: updatedMetadatas.map { SendableItemMetadata(value: $0) },
            updateDirectoryEtags: true,
            keepExistingDownloadState: true
        )

        let remainingMetadatas = Self.dbManager.itemMetadatas(
            account: "TestAccount", underServerUrl: "https://example.com"
        )
        XCTAssertEqual(
            remainingMetadatas.count, 2, "Should have two remaining metadata after update"
        )

        XCTAssertNotNil(remainingMetadatas.first { $0.ocId == "id-1" })
        XCTAssertNotNil(remainingMetadatas.first { $0.ocId == "id-3" })
    }

    func testProcessItemMetadatasToUpdate_NewAndUpdatedSeparation() throws {
        let account = Account(
            user: "TestAccount", id: "taid", serverUrl: "https://example.com", password: "pass"
        )

        // Simulate existing metadata in the database
        let existingMetadata = RealmItemMetadata()
        existingMetadata.ocId = "id-1"
        existingMetadata.fileName = "File.pdf"
        existingMetadata.account = "TestAccount"
        existingMetadata.serverUrl = "https://example.com"
        existingMetadata.downloaded = true
        existingMetadata.uploaded = true

        // Simulate updated metadata that includes changes and a new entry
        let updatedMetadata =
            SendableItemMetadata(ocId: "id-1", fileName: "UpdatedFile.pdf", account: account)

        let newMetadata =
            SendableItemMetadata(ocId: "id-2", fileName: "NewFile.pdf", account: account)

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(existingMetadata)
        }

        let results = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: "TestAccount",
            serverUrl: "https://example.com",
            updatedMetadatas: [updatedMetadata, newMetadata],
            updateDirectoryEtags: true,
            keepExistingDownloadState: true
        )

        XCTAssertEqual(results.newMetadatas?.count, 1, "Should create one new metadata")
        XCTAssertEqual(results.updatedMetadatas?.count, 1, "Should update one existing metadata")
        XCTAssertEqual(
            results.newMetadatas?.first?.ocId, "id-2", "New metadata ocId should be 'id-2'"
        )
        XCTAssertEqual(
            results.updatedMetadatas?.first?.fileName,
            "UpdatedFile.pdf",
            "Updated metadata should have the new file name"
        )
    }

    func testUnuploadedItemsAreNotDeletedDuringUpdate() throws {
        let testAccount = "TestAccount"
        let testServerUrl = "https://example.com/files/"

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

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(uploadedItem)
            realm.add(unuploadedItem)
        }

        // Verify initial state (optional but good practice)
        XCTAssertEqual(realm.objects(RealmItemMetadata.self).where { $0.account == testAccount && $0.serverUrl == testServerUrl }.count, 2)

        // Simulate an update from the server that contains NEITHER of these items.
        // This means the server thinks 'SyncedFile.txt' was deleted,
        // and it doesn't know about 'NewLocalFile.txt' yet.
        let updatedMetadatasFromServer: [SendableItemMetadata] = []

        let results = Self.dbManager.depth1ReadUpdateItemMetadatas(
            account: testAccount,
            serverUrl: testServerUrl,
            updatedMetadatas: updatedMetadatasFromServer,
            updateDirectoryEtags: true, // Value doesn't strictly matter for this test logic
            keepExistingDownloadState: true // Value doesn't strictly matter for this test logic
        )

        // --- Assertion ---
        let remainingMetadatas = realm.objects(RealmItemMetadata.self)
            .where { $0.account == testAccount && $0.serverUrl == testServerUrl }

        // Check the returned delete list (based on the copy made before deletion)
        XCTAssertEqual(results.deletedMetadatas?.count, 1, "Should identify the uploaded item as deleted.")
        XCTAssertEqual(results.deletedMetadatas?.first?.ocId, "ocid-uploaded-123", "The correct uploaded item should be marked for deletion.")
        XCTAssertTrue(results.deletedMetadatas?.first?.uploaded ?? false, "The item marked for deletion should have uploaded=true")


        // Check the actual database state after the write transaction
        XCTAssertEqual(remainingMetadatas.count, 1, "Only one item should remain in the database.")

        let survivingItem = remainingMetadatas.first
        XCTAssertNotNil(survivingItem, "An item should survive.")
        XCTAssertEqual(survivingItem?.ocId, "ocid-local-456", "The surviving item should be the unuploaded one.")
        XCTAssertEqual(survivingItem?.fileName, "NewLocalFile.txt", "Filename should match the unuploaded item.")
        XCTAssertFalse(survivingItem!.uploaded, "The surviving item must be the one marked uploaded = false.")

         // Check other return values are empty as expected
        XCTAssertTrue(results.newMetadatas?.isEmpty ?? true, "No new items should have been created.")
        XCTAssertTrue(results.updatedMetadatas?.isEmpty ?? true, "No items should have been updated.")
    }

    func testConcurrencyOnDatabaseWrites() {
        let semaphore = DispatchSemaphore(value: 0)
        let count = 100
        Task {
            for i in 0...count {
                let metadata = SendableItemMetadata(
                    ocId: "concurrency-\(i)",
                    fileName: "name",
                    account: Account(user: "", id: "", serverUrl: "", password: "")
                )
                Self.dbManager.addItemMetadata(metadata)
            }
            semaphore.signal()
        }

        Task {
            for i in 0...count {
                let metadata = SendableItemMetadata(
                    ocId: "concurrency-\(count + 1 + i)",
                    fileName: "name",
                    account: Account(user: "", id: "", serverUrl: "", password: "")
                )
                Self.dbManager.addItemMetadata(metadata)
            }
            semaphore.signal()
        }

        semaphore.wait()
        semaphore.wait()

        for i in 0...count * 2 + 1 {
            let resultsI = Self.dbManager.itemMetadata(ocId: "concurrency-\(i)")
            XCTAssertNotNil(resultsI, "Metadata \(i) should be saved even under concurrency")
        }
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

    func testErrorOnDirectoryMetadataNotFound() throws {
        let nonExistentServerUrl = "https://cloud.example.com/nonexistent"
        let directoryMetadata = Self.dbManager.itemMetadata(
            account: "TestAccount", locatedAtRemoteUrl: nonExistentServerUrl
        )
        XCTAssertNil(directoryMetadata, "Should return nil when directory metadata is not found")
    }

    func testChildItemsForRootDirectory() throws {
        var rootMetadata = SendableItemMetadata(
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
            "Should update serverUrl of all child items to reflect new directory path")
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

    func testInitializerMigration() throws {
        let account1 = Account(user: "account1", id: "account1", serverUrl: "c.nc.c", password: "a")
        let account2 = Account(user: "account2", id: "account2", serverUrl: "c.nc.c", password: "b")

        // 1. Create a unique temporary directory for the file provider data directory.
        let tempDir = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: tempDir, withIntermediateDirectories: true)

        // 2. Define a custom relative database folder path.
        // For example, if you normally use "Nextcloud/Realm/", here we use "Test/Realm/".
        let customRelativeDatabaseFolderPath = "Test/Realm/"

        // 3. Build the expected old Realm file URL using the custom relative path
        // and the class’s databaseFilename.
        let oldRealmURL = tempDir.appendingPathComponent(
            customRelativeDatabaseFolderPath + databaseFilename
        )
        try FileManager.default.createDirectory(
            at: oldRealmURL.deletingLastPathComponent(), withIntermediateDirectories: true
        )

        // 4. Create the old Realm configuration and insert test objects.
        // Use stable2_0SchemaVersion and the appropriate object types.
        let oldConfig = Realm.Configuration(
            fileURL: oldRealmURL,
            schemaVersion: stable2_0SchemaVersion,
            objectTypes: [RealmItemMetadata.self, RemoteFileChunk.self]
        )
        let oldRealm = try Realm(configuration: oldConfig)

        // Create test objects
        let migratingItem = RealmItemMetadata()
        migratingItem.ocId = "id1"
        migratingItem.account = account1.ncKitAccount

        let nonMigratingItem = RealmItemMetadata()
        nonMigratingItem.ocId = "id2"
        nonMigratingItem.account = account2.ncKitAccount

        let remoteChunk = RemoteFileChunk()
        remoteChunk.fileName = "chunk1"

        try oldRealm.write {
            oldRealm.add(migratingItem)
            oldRealm.add(nonMigratingItem)
            oldRealm.add(remoteChunk)
        }

        XCTAssertEqual(oldRealm.objects(RealmItemMetadata.self).count, 2)
        XCTAssertEqual(oldRealm.objects(RemoteFileChunk.self).count, 1)

        // 5. Prepare a new Realm configuration for the target per‑account database.
        let newRealmURL = tempDir.appendingPathComponent("new.realm")
        let newConfig = Realm.Configuration(
            fileURL: newRealmURL,
            schemaVersion: stable2_0SchemaVersion,
            objectTypes: [RealmItemMetadata.self, RemoteFileChunk.self]
        )

        // 6. Call the initializer that performs the migration.
        // It will search for the old database at:
        // fileProviderDataDirUrl/appendingPathComponent(customRelativeDbFolderPath + dbFilename)
        // and migrate only metadata objects with account == "account1" plus remote file chunks.
        let dbManager = FilesDatabaseManager(
            realmConfig: newConfig,
            account: account1,
            fileProviderDataDirUrl: tempDir,
            relativeDatabaseFolderPath: customRelativeDatabaseFolderPath
        )

        // 7. Verify that the new Realm now contains the migrated objects.
        let newRealm = dbManager.ncDatabase()
        let newMigratedItems = newRealm.objects(RealmItemMetadata.self)
        let newRemoteChunks = newRealm.objects(RemoteFileChunk.self)
        XCTAssertEqual(
            newMigratedItems.count, 1, "Only one metadata item for account1 should be migrated"
        )
        XCTAssertEqual(newMigratedItems.first?.account, account1.ncKitAccount)
        XCTAssertEqual(newRemoteChunks.count, 1, "Remote file chunks should be migrated")

        // 8. Verify that the old Realm has retained the migrated objects.
        let oldRealmAfter = try Realm(configuration: oldConfig)
        let allItems = oldRealmAfter.objects(RealmItemMetadata.self)
        let allChunks = oldRealmAfter.objects(RemoteFileChunk.self)

        // Expect both metadata objects still to be present.
        XCTAssertEqual(
            allItems.count,
            2,
            "The old realm should have retained both migrated and non-migrated metadata items"
        )
        XCTAssertTrue(
            allItems.contains(where: { $0.account == account1.ncKitAccount }),
            "Migrated metadata should be retained in the old realm"
        )
        XCTAssertEqual(
            allChunks.count, 1, "The remote file chunks should be retained in the old realm"
        )


        // 9. Clean up by removing the temporary directory.
        try FileManager.default.removeItem(at: tempDir)
    }

    func testMigrationIsNotPerformedTwice() throws {
        let fm = FileManager.default
        let tempDir = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try fm.createDirectory(at: tempDir, withIntermediateDirectories: true)

        let customRelativePath = "Test/Realm/"
        let oldRealmURL = tempDir.appendingPathComponent(
            customRelativePath + NextcloudFileProviderKit.databaseFilename
        )
        try fm.createDirectory(
            at: oldRealmURL.deletingLastPathComponent(), withIntermediateDirectories: true
        )

        let account1 = Account(user: "account1", id: "account1", serverUrl: "c.nc.c", password: "a")
        let account2 = Account(user: "account2", id: "account2", serverUrl: "c.nc.c", password: "b")
        let accounts = (migrated: account1, nonMigrated: account2)

        // Insert initial objects into the old realm
        let oldConfig = Realm.Configuration(
            fileURL: oldRealmURL,
            schemaVersion: stable2_0SchemaVersion,
            objectTypes: [RealmItemMetadata.self, RemoteFileChunk.self]
        )
        let oldRealm = try Realm(configuration: oldConfig)

        let migratedItem = RealmItemMetadata()
        migratedItem.ocId = "id1"
        migratedItem.account = accounts.migrated.ncKitAccount

        let nonMigratedItem = RealmItemMetadata()
        nonMigratedItem.ocId = "id2"
        nonMigratedItem.account = accounts.nonMigrated.ncKitAccount

        let remoteChunk = RemoteFileChunk()
        remoteChunk.fileName = "chunk1"

        try oldRealm.write {
            oldRealm.add(migratedItem)
            oldRealm.add(nonMigratedItem)
            oldRealm.add(remoteChunk)
        }

        // New realm config (target)
        let newRealmURL = tempDir.appendingPathComponent("new.realm")
        let newConfig = Realm.Configuration(
            fileURL: newRealmURL,
            schemaVersion: stable2_0SchemaVersion,
            objectTypes: [RealmItemMetadata.self, RemoteFileChunk.self]
        )

        // First migration
        let newDbManager = FilesDatabaseManager(
            realmConfig: newConfig,
            account: accounts.migrated,
            fileProviderDataDirUrl: tempDir,
            relativeDatabaseFolderPath: customRelativePath
        )

        let newRealm = newDbManager.ncDatabase()
        let initialMigrated = newRealm.objects(RealmItemMetadata.self)
        let initialChunks = newRealm.objects(RemoteFileChunk.self)
        XCTAssertEqual(initialMigrated.count, 1)
        XCTAssertEqual(initialMigrated.first?.account, accounts.migrated.ncKitAccount)
        XCTAssertEqual(initialChunks.count, 1)

        // Add additional migrated objects to the old realm after the first migration
        let newMigratedItem = RealmItemMetadata()
        newMigratedItem.ocId = "id3"
        newMigratedItem.account = accounts.migrated.ncKitAccount

        let newRemoteChunk = RemoteFileChunk()
        newRemoteChunk.fileName = "chunk2"

        try oldRealm.write {
            oldRealm.add(newMigratedItem)
            oldRealm.add(newRemoteChunk)
        }

        // Second migration call; new objects added after the first migration must not be added
        let secondNewDbManager = FilesDatabaseManager(
            realmConfig: newConfig,
            account: accounts.migrated,
            fileProviderDataDirUrl: tempDir,
            relativeDatabaseFolderPath: customRelativePath
        )

        let secondNewRealm = secondNewDbManager.ncDatabase()
        let finalMigrated = secondNewRealm.objects(RealmItemMetadata.self)
        let finalChunks = secondNewRealm.objects(RemoteFileChunk.self)
        XCTAssertEqual(finalMigrated.count, 1, "Migration should occur only once")
        XCTAssertEqual(finalChunks.count, 1, "Migration should occur only once")

        try FileManager.default.removeItem(at: tempDir)
    }

    func testKeepOfflineSetting() throws {
        let existingMetadata = RealmItemMetadata()
        existingMetadata.ocId = "id-1"
        existingMetadata.fileName = "File.pdf"
        existingMetadata.account = "TestAccount"
        existingMetadata.serverUrl = "https://example.com"
        XCTAssertFalse(existingMetadata.keepOffline)

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(existingMetadata)
        }

        let sendable = SendableItemMetadata(value: existingMetadata)
        var updatedMetadata =
            try XCTUnwrap(try Self.dbManager.set(keepOffline: true, for: sendable))
        XCTAssertTrue(updatedMetadata.keepOffline)

        updatedMetadata.keepOffline = false
        let finalMetadata =
            try XCTUnwrap(try Self.dbManager.set(keepOffline: false, for: updatedMetadata))
        XCTAssertFalse(finalMetadata.keepOffline)
    }
}
