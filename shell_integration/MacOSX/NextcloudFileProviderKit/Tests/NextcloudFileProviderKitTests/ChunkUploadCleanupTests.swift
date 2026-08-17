//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import TestInterface
import XCTest

final class ChunkUploadCleanupTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser",
        id: "testUserId",
        serverUrl: "https://mock.nc.com",
        password: "abcd"
    )

    private var dbManager: FilesDatabaseManager!
    private var remoteInterface: MockRemoteInterface!
    private var temporaryChunkDirectories: [URL] = []

    private func assertChunkCount(
        for uploadIdentifier: String,
        equals expectedCount: Int,
        file: StaticString = #filePath,
        line: UInt = #line
    ) {
        let count = dbManager.ncDatabase().objects(RemoteFileChunk.self)
            .where { $0.remoteChunkStoreFolderName == uploadIdentifier }
            .count
        XCTAssertEqual(count, expectedCount, file: file, line: line)
    }

    private func makeLogger() -> FileProviderLogger {
        FileProviderLogger(category: "ChunkUploadCleanupTests", log: FileProviderLogMock())
    }

    override func setUp() {
        super.setUp()
        dbManager = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier(name),
            log: FileProviderLogMock()
        )
        remoteInterface = MockRemoteInterface(
            account: Self.account,
            rootItem: MockRemoteItem.rootItem(account: Self.account)
        )
    }

    override func tearDown() {
        for directory in temporaryChunkDirectories {
            try? FileManager.default.removeItem(at: directory)
        }
        temporaryChunkDirectories.removeAll()
        super.tearDown()
    }

    func testStartupCleanupRemovesDeletedItemUpload() throws {
        let seeded = try seedChunkUpload(
            itemIdentifier: "deleted-item",
            metadataStatus: .uploadError,
            metadataDeleted: true
        )

        cleanupAbandonedChunkUploads(
            usingRemoteInterface: remoteInterface,
            dbManager: dbManager,
            logger: makeLogger()
        )

        assertUploadRemoved(seeded)
    }

    func testStartupCleanupPreservesResumableUpload() throws {
        let seeded = try seedChunkUpload(
            itemIdentifier: "uploading-item",
            metadataStatus: .uploading
        )

        cleanupAbandonedChunkUploads(
            usingRemoteInterface: remoteInterface,
            dbManager: dbManager,
            logger: makeLogger()
        )

        XCTAssertTrue(FileManager.default.fileExists(atPath: seeded.directory.path))
        XCTAssertEqual(
            dbManager.itemMetadata(ocId: seeded.itemIdentifier)?.chunkUploadId,
            seeded.uploadIdentifier
        )
        assertChunkCount(for: seeded.uploadIdentifier, equals: 1)
    }

    func testStartupCleanupRemovesUploadForItemInNormalState() throws {
        let seeded = try seedChunkUpload(
            itemIdentifier: "completed-item",
            metadataStatus: .normal
        )

        cleanupAbandonedChunkUploads(
            usingRemoteInterface: remoteInterface,
            dbManager: dbManager,
            logger: makeLogger()
        )

        assertUploadRemoved(seeded)
    }

    func testStartupCleanupRemovesUploadWithoutMetadata() throws {
        let seeded = try seedChunkUpload(
            itemIdentifier: "abandoned-new-item",
            metadataStatus: nil
        )

        cleanupAbandonedChunkUploads(
            usingRemoteInterface: remoteInterface,
            dbManager: dbManager,
            logger: makeLogger()
        )

        assertUploadRemoved(seeded)
    }

    func testStartupCleanupUsesMetadataIdentifierWhenNoChunkRowsRemain() throws {
        let seeded = try seedChunkUpload(
            itemIdentifier: "completed-chunks-item",
            metadataStatus: .normal,
            includeChunkRow: false
        )

        cleanupAbandonedChunkUploads(
            usingRemoteInterface: remoteInterface,
            dbManager: dbManager,
            logger: makeLogger()
        )

        assertUploadRemoved(seeded)
    }

    func testStartupCleanupRemovesSupersededUploadAndPreservesCurrentUpload() throws {
        let stale = try seedChunkUpload(
            uploadIdentifier: "superseded-upload",
            itemIdentifier: "uploading-item",
            metadataStatus: .uploading,
            associateWithMetadata: false
        )
        let current = try seedChunkUpload(
            uploadIdentifier: "current-upload",
            itemIdentifier: "uploading-item",
            metadataStatus: .uploading
        )

        cleanupAbandonedChunkUploads(
            usingRemoteInterface: remoteInterface,
            dbManager: dbManager,
            logger: makeLogger()
        )

        assertUploadRemoved(stale)
        XCTAssertTrue(FileManager.default.fileExists(atPath: current.directory.path))
        XCTAssertEqual(
            dbManager.itemMetadata(ocId: current.itemIdentifier)?.chunkUploadId,
            current.uploadIdentifier
        )
        assertChunkCount(for: current.uploadIdentifier, equals: 1)
    }

    func testStartupCleanupRetainsBookkeepingWhenLocalRemovalFails() throws {
        let seeded = try seedChunkUpload(
            itemIdentifier: "deleted-item",
            metadataStatus: .uploadError,
            metadataDeleted: true
        )
        remoteInterface.removeLocalChunksError = CocoaError(.fileWriteNoPermission)

        cleanupAbandonedChunkUploads(
            usingRemoteInterface: remoteInterface,
            dbManager: dbManager,
            logger: makeLogger()
        )

        XCTAssertTrue(FileManager.default.fileExists(atPath: seeded.directory.path))
        XCTAssertEqual(
            dbManager.itemMetadata(ocId: seeded.itemIdentifier)?.chunkUploadId,
            seeded.uploadIdentifier
        )
        assertChunkCount(for: seeded.uploadIdentifier, equals: 1)
    }

    func testCompletedUploadCleanupIsRetriedFromDurableRecord() throws {
        let uploadIdentifier = "completed-upload-with-failed-cleanup"
        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("pending-cleanup-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        temporaryChunkDirectories.append(directory)
        try Data([1]).write(to: directory.appendingPathComponent("1"))
        remoteInterface.chunkUploadDirectories[uploadIdentifier] = directory
        remoteInterface.removeLocalChunksError = CocoaError(.fileWriteNoPermission)

        removeLocalChunkUpload(
            uploadIdentifier: uploadIdentifier,
            chunksDirectory: nil,
            usingRemoteInterface: remoteInterface,
            dbManager: dbManager,
            logger: makeLogger()
        )

        XCTAssertTrue(FileManager.default.fileExists(atPath: directory.path))
        XCTAssertEqual(
            dbManager.ncDatabase().objects(RealmPendingChunkUploadCleanup.self)
                .where { $0.uploadIdentifier == uploadIdentifier }
                .count,
            1
        )

        remoteInterface.removeLocalChunksError = nil
        cleanupAbandonedChunkUploads(
            usingRemoteInterface: remoteInterface,
            dbManager: dbManager,
            logger: makeLogger()
        )

        XCTAssertFalse(FileManager.default.fileExists(atPath: directory.path))
        XCTAssertEqual(
            dbManager.ncDatabase().objects(RealmPendingChunkUploadCleanup.self)
                .where { $0.uploadIdentifier == uploadIdentifier }
                .count,
            0
        )
    }

    func testDiscardChunkUploadsDoesNotMatchSimilarItemIdentifiers() throws {
        for (itemIdentifier, similarItemIdentifier) in [("a", "a_b"), ("a/b", "ab")] {
            let itemUploadIdentifier = chunkUploadIdentifier(
                forItemWithIdentifier: itemIdentifier,
                fileSize: 1,
                modificationDate: Date(timeIntervalSince1970: 1_700_000_000)
            )
            let similarItemUploadIdentifier = chunkUploadIdentifier(
                forItemWithIdentifier: similarItemIdentifier,
                fileSize: 1,
                modificationDate: Date(timeIntervalSince1970: 1_700_000_000)
            )
            let itemUpload = try seedChunkUpload(
                uploadIdentifier: itemUploadIdentifier,
                itemIdentifier: itemIdentifier,
                metadataStatus: .normal
            )
            let similarItemUpload = try seedChunkUpload(
                uploadIdentifier: similarItemUploadIdentifier,
                itemIdentifier: similarItemIdentifier,
                metadataStatus: .normal
            )

            discardChunkUploads(
                forItemIdentifiers: [itemIdentifier],
                usingRemoteInterface: remoteInterface,
                dbManager: dbManager,
                logger: makeLogger()
            )

            XCTAssertFalse(FileManager.default.fileExists(atPath: itemUpload.directory.path))
            XCTAssertTrue(FileManager.default.fileExists(atPath: similarItemUpload.directory.path))
            assertChunkCount(for: itemUploadIdentifier, equals: 0)
            assertChunkCount(for: similarItemUploadIdentifier, equals: 1)
        }
    }

    private func seedChunkUpload(
        uploadIdentifier requestedUploadIdentifier: String? = nil,
        itemIdentifier: String,
        metadataStatus: Status?,
        metadataDeleted: Bool = false,
        associateWithMetadata: Bool = true,
        includeChunkRow: Bool = true
    ) throws -> (uploadIdentifier: String, itemIdentifier: String, directory: URL) {
        let uploadIdentifier = requestedUploadIdentifier ?? "\(itemIdentifier)-upload"

        if let metadataStatus {
            var metadata = SendableItemMetadata(
                ocId: itemIdentifier,
                fileName: "file.txt",
                account: Self.account
            )
            metadata.status = metadataStatus.rawValue
            metadata.deleted = metadataDeleted
            metadata.chunkUploadId = associateWithMetadata ? uploadIdentifier : nil
            dbManager.addItemMetadata(metadata)
        }

        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("chunk-cleanup-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        temporaryChunkDirectories.append(directory)
        try Data([1]).write(to: directory.appendingPathComponent("1"))
        remoteInterface.chunkUploadDirectories[uploadIdentifier] = directory

        if includeChunkRow {
            let db = dbManager.ncDatabase()
            try db.write {
                db.add(RemoteFileChunk(
                    fileName: "1",
                    size: 1,
                    remoteChunkStoreFolderName: uploadIdentifier
                ))
            }
        }

        return (uploadIdentifier, itemIdentifier, directory)
    }

    private func assertUploadRemoved(
        _ seeded: (uploadIdentifier: String, itemIdentifier: String, directory: URL),
        file: StaticString = #filePath,
        line: UInt = #line
    ) {
        XCTAssertFalse(
            FileManager.default.fileExists(atPath: seeded.directory.path),
            file: file,
            line: line
        )
        XCTAssertNotEqual(
            dbManager.itemMetadata(ocId: seeded.itemIdentifier)?.chunkUploadId,
            seeded.uploadIdentifier,
            file: file,
            line: line
        )
        assertChunkCount(for: seeded.uploadIdentifier, equals: 0, file: file, line: line)
    }
}
