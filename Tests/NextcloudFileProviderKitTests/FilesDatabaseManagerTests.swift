//
//  FilesDatabaseManagerTests.swift
//
//
//  Created by Claudio Cambra on 15/5/24.
//

import Foundation
import RealmSwift
import XCTest
@testable import NextcloudFileProviderKit

final class FilesDatabaseManagerTests: XCTestCase {
    static let account = Account(
        user: "testUser", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    static let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

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
        let metadata = ItemMetadata()
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
        let metadata = ItemMetadata()
        metadata.ocId = ocId

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        let fetchedMetadata = Self.dbManager.itemMetadataFromOcId(ocId)
        XCTAssertNotNil(fetchedMetadata, "Should fetch metadata with the specified ocId")
        XCTAssertEqual(
            fetchedMetadata?.ocId, ocId, "Fetched metadata ocId should match the requested ocId"
        )
    }

    func testUpdateItemMetadatas() {
        // Setting up test data
        let testAccount = "TestAccount"
        let serverUrl = "https://example.com"
        let metadata = ItemMetadata()
        metadata.account = testAccount
        metadata.serverUrl = serverUrl

        // Call updateItemMetadatas
        let result = Self.dbManager.updateItemMetadatas(
            account: testAccount,
            serverUrl: serverUrl,
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
        let metadata = ItemMetadata()
        metadata.ocId = "unique-id-123"
        metadata.status = ItemMetadata.Status.normal.rawValue

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        let expectedStatus = ItemMetadata.Status.uploadError
        Self.dbManager.setStatusForItemMetadata(metadata, status: expectedStatus) { updatedMetadata in
            XCTAssertEqual(
                updatedMetadata?.status,
                expectedStatus.rawValue,
                "Status should be updated to \(expectedStatus)"
            )
        }
    }

    func testAddItemMetadata() {
        let metadata = ItemMetadata()
        metadata.ocId = "unique-id-123"
        Self.dbManager.addItemMetadata(metadata)

        let fetchedMetadata = Self.dbManager.itemMetadataFromOcId("unique-id-123")
        XCTAssertNotNil(fetchedMetadata, "Metadata should be added to the database")
    }

    func testDeleteItemMetadata() throws {
        let ocId = "unique-id-123"
        let metadata = ItemMetadata()
        metadata.ocId = ocId

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata)
        }

        let result = Self.dbManager.deleteItemMetadata(ocId: ocId)
        XCTAssertTrue(result, "deleteItemMetadata should return true on successful deletion")
        XCTAssertNil(
            Self.dbManager.itemMetadataFromOcId(ocId),
            "Metadata should be deleted from the database"
        )
    }

    func testRenameItemMetadata() throws {
        let ocId = "unique-id-123"
        let newFileName = "newFileName.pdf"
        let newServerUrl = "https://new.example.com"
        let metadata = ItemMetadata()
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

        let updatedMetadata = Self.dbManager.itemMetadataFromOcId(ocId)
        XCTAssertEqual(updatedMetadata?.fileName, newFileName, "File name should be updated")
        XCTAssertEqual(updatedMetadata?.serverUrl, newServerUrl, "Server URL should be updated")
    }

    func testItemMetadatasWithStatus() throws {
        // Setup metadatas with different statuses
        let metadata1 = ItemMetadata()
        metadata1.ocId = "id-1"
        metadata1.account = "TestAccount"
        metadata1.serverUrl = "https://example.com"
        metadata1.status = ItemMetadata.Status.waitDownload.rawValue

        let metadata2 = ItemMetadata()
        metadata2.ocId = "id-2"
        metadata2.account = "TestAccount"
        metadata2.serverUrl = "https://example.com"
        metadata2.status = ItemMetadata.Status.normal.rawValue

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(metadata1)
            realm.add(metadata2)
        }

        let results = Self.dbManager.itemMetadatas(
            account: "TestAccount", serverUrl: "https://example.com", status: .waitDownload
        )
        XCTAssertEqual(results.count, 1, "Should return only metadatas with 'waitDownload' status")
        XCTAssertEqual(
            results.first?.ocId,
            "id-1",
            "The ocId should match the metadata with 'waitDownload' status"
        )
    }

    func testDeleteItemMetadatasBasedOnUpdate() throws {
        // Existing metadata in the database
        let existingMetadata1 = ItemMetadata()
        existingMetadata1.ocId = "id-1"
        existingMetadata1.fileName = "Existing.pdf"
        existingMetadata1.serverUrl = "https://example.com"
        existingMetadata1.account = "TestAccount"

        let existingMetadata2 = ItemMetadata()
        existingMetadata2.ocId = "id-2"
        existingMetadata2.fileName = "Existing2.pdf"
        existingMetadata2.serverUrl = "https://example.com"
        existingMetadata2.account = "TestAccount"

        let realm = Self.dbManager.ncDatabase()
        try realm.write {
            realm.add(existingMetadata1)
            realm.add(existingMetadata2)
        }

        // Simulate updated metadata that leads to a deletion
        let updatedMetadatas = [existingMetadata1]  // Only include one of the two

        let _ = Self.dbManager.updateItemMetadatas(
            account: "TestAccount",
            serverUrl: "https://example.com",
            updatedMetadatas: updatedMetadatas,
            updateDirectoryEtags: true
        )

        let remainingMetadatas = Self.dbManager.itemMetadatas(
            account: "TestAccount", serverUrl: "https://example.com"
        )
        XCTAssertEqual(
            remainingMetadatas.count, 1, "Should have one remaining metadata after update"
        )
        XCTAssertEqual(
            remainingMetadatas.first?.ocId,
            "id-1",
            "Remaining metadata should be the one that was included in the update"
        )
    }

    func testProcessItemMetadatasToUpdate_NewAndUpdatedSeparation() throws {
        // Simulate existing metadata in the database
        let existingMetadata = ItemMetadata()
        existingMetadata.ocId = "id-1"
        existingMetadata.fileName = "File.pdf"
        existingMetadata.account = "TestAccount"
        existingMetadata.serverUrl = "https://example.com"

        // Simulate updated metadata that includes changes and a new entry
        let updatedMetadata = ItemMetadata()
        updatedMetadata.ocId = "id-1"
        updatedMetadata.fileName = "UpdatedFile.pdf"  // Update existing
        updatedMetadata.account = "TestAccount"
        updatedMetadata.serverUrl = "https://example.com"

        let newMetadata = ItemMetadata()
        newMetadata.ocId = "id-2"
        newMetadata.fileName = "NewFile.pdf"  // This is a new entry
        newMetadata.account = "TestAccount"
        newMetadata.serverUrl = "https://example.com"

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
                let ocId = "concurrency-\(i)"
                let metadata = ItemMetadata()
                metadata.ocId = ocId
                Self.dbManager.addItemMetadata(metadata)
            }
            semaphore.signal()
        }

        Task {
            for i in 0...count {
                let ocId = "concurrency-\(count + 1 + i)"
                let metadata = ItemMetadata()
                metadata.ocId = ocId
                Self.dbManager.addItemMetadata(metadata)
            }
            semaphore.signal()
        }

        semaphore.wait()
        semaphore.wait()

        for i in 0...count * 2 + 1 {
            let resultsI = Self.dbManager.itemMetadataFromOcId("concurrency-\(i)")
            XCTAssertNotNil(resultsI, "Metadata \(i) should be saved even under concurrency")
        }
    }
}
