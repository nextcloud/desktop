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
        realmConfig: .defaultConfiguration, account: account.ncKitAccount
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
        // Setting up test data
        let account = Account(user: "test", id: "t", serverUrl: "https://example.com", password: "")
        let metadata = SendableItemMetadata(ocId: "test", fileName: "test", account: account)

        // Call updateItemMetadatas
        let result = Self.dbManager.updateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [metadata],
            updateDirectoryEtags: true
        )

        XCTAssertNotNil(result.newMetadatas, "Should create new metadatas")
        XCTAssertTrue(
            result.updatedMetadatas?.isEmpty ?? false, "No existing metadata should be updated"
        )
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

        let existingMetadata2 = RealmItemMetadata()
        existingMetadata2.ocId = "id-2"
        existingMetadata2.fileName = "Existing2.pdf"
        existingMetadata2.serverUrl = "https://example.com"
        existingMetadata2.account = "TestAccount"

        let existingMetadata3 = RealmItemMetadata()
        existingMetadata3.ocId = "id-3"
        existingMetadata3.fileName = "Existing3.pdf"
        existingMetadata3.serverUrl = "https://example.com/folder" // Different child path
        existingMetadata3.account = "TestAccount"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(existingMetadata1)
            realm.add(existingMetadata2)
            realm.add(existingMetadata3)
        }

        // Simulate updated metadata that leads to a deletion
        let updatedMetadatas = [existingMetadata1, existingMetadata3]  // Only include 2 of the 3

        let _ = Self.dbManager.updateItemMetadatas(
            account: "TestAccount",
            serverUrl: "https://example.com",
            updatedMetadatas: updatedMetadatas.map { SendableItemMetadata(value: $0) },
            updateDirectoryEtags: true
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

        // Simulate updated metadata that includes changes and a new entry
        let updatedMetadata =
            SendableItemMetadata(ocId: "id-1", fileName: "UpdatedFile.pdf", account: account)

        let newMetadata =
            SendableItemMetadata(ocId: "id-2", fileName: "NewFile.pdf", account: account)

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(existingMetadata)
        }

        let results = Self.dbManager.updateItemMetadatas(
            account: "TestAccount",
            serverUrl: "https://example.com",
            updatedMetadatas: [updatedMetadata, newMetadata],
            updateDirectoryEtags: true
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
}
