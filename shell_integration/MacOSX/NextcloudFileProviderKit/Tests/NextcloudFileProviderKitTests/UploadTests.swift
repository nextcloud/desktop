//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import TestInterface
import XCTest

final class UploadTests: NextcloudFileProviderKitTestCase {
    static let account = Account(user: "user", id: "id", serverUrl: "test.cloud.com", password: "1234")
    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        setUpDatabase(Self.dbManager)
    }

    func testCancellationErrorMappingForCollisions() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        for code in [NSURLErrorCancelled, -5] {
            let error = NKError(errorCode: code, errorDescription: "Cancelled")
            let mappedError = await error.fileProviderError(
                handlingCollisionAgainstItemInRemotePath: Self.account.davFilesUrl + "/file.txt",
                dbManager: Self.dbManager,
                remoteInterface: remoteInterface,
                log: FileProviderLogMock()
            )
            XCTAssertEqual((mappedError as? CocoaError)?.code, .userCancelled)
        }
    }

    func testCancellationDuringChunkPreparationReturnsCancellation() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data(repeating: 1, count: 8).write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }
        let root = MockRemoteItem.rootItem(account: Self.account)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: root)
        let progress = Progress()
        remoteInterface.chunkPreparationHandler = {
            progress.cancel()
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Chunk preparation must receive cancellation")
        }

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: 3,
            forItemWithIdentifier: UUID().uuidString,
            dbManager: Self.dbManager,
            progress: progress,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError.errorCode, NSURLErrorCancelled)
        XCTAssertEqual((result.remoteError.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: .init("file")) as? CocoaError)?.code, .userCancelled)
        XCTAssertNil(result.ocId)
        XCTAssertTrue(progress.isCancelled)
        XCTAssertEqual(remoteInterface.uploadOperationCount, 0)
        XCTAssertTrue(root.children.isEmpty)
        XCTAssertTrue(remoteInterface.chunkUploadDirectories.isEmpty)
    }

    func testChunkPreparationErrorWithoutCancellationIsPreserved() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data(repeating: 1, count: 8).write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let preparationError = NKError(errorCode: -4, errorDescription: "Could not write chunks.")
        remoteInterface.chunkPreparationError = preparationError

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: 3,
            forItemWithIdentifier: UUID().uuidString,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError.errorCode, preparationError.errorCode)
        XCTAssertEqual(result.remoteError.errorDescription, preparationError.errorDescription)
        XCTAssertEqual(remoteInterface.uploadOperationCount, 0)
    }

    func testCancellationAfterChunkAssemblyPreservesResult() async throws {
        for remoteError in [NKError.success, .errorChunkMoveFile, NKError(errorCode: 507, errorDescription: "Insufficient quota.")] {
            let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
            let data = Data(repeating: 1, count: 8)
            try data.write(to: fileUrl)
            defer { try? FileManager.default.removeItem(at: fileUrl) }
            let root = MockRemoteItem.rootItem(account: Self.account)
            let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: root)
            let progress = Progress()
            remoteInterface.chunkAssemblyHandler = {
                progress.cancel()
                let cancelled = await waitForCancellation()
                XCTAssertTrue(cancelled, "Assembly must receive cancellation")
            }
            if remoteError != .success {
                remoteInterface.uploadError = remoteError
            }

            let result = await NextcloudFileProviderKit.upload(
                fileLocatedAt: fileUrl.path,
                toRemotePath: Self.account.davFilesUrl + "/file.txt",
                usingRemoteInterface: remoteInterface,
                withAccount: Self.account,
                inChunksSized: 3,
                forItemWithIdentifier: UUID().uuidString,
                dbManager: Self.dbManager,
                progress: progress,
                log: FileProviderLogMock()
            )

            XCTAssertTrue(progress.isCancelled)
            XCTAssertEqual(result.remoteError.errorCode, remoteError.errorCode)
            XCTAssertEqual(remoteInterface.uploadOperationCount, 1)
            if remoteError == .success {
                let uploaded = try XCTUnwrap(root.children.first)
                XCTAssertEqual(uploaded.data, data)
                XCTAssertEqual(result.ocId, uploaded.identifier)
                XCTAssertEqual(result.etag, uploaded.versionIdentifier)
                XCTAssertEqual(result.size, Int64(data.count))
            } else {
                XCTAssertTrue(root.children.isEmpty)
            }
        }
    }

    func testStandardUpload() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            forItemWithIdentifier: "standard-upload-item",
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)
    }

    func testChunkedUpload() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let chunkSize = 3
        let itemIdentifier = "chunked-upload-\(UUID().uuidString)"
        let chunkDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("mock-chunks-\(UUID().uuidString)", isDirectory: true)
        remoteInterface.chunkUploadDirectory = chunkDirectory
        defer { try? FileManager.default.removeItem(at: chunkDirectory) }

        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { chunk in
                uploadedChunks.record(chunk, fileExists: FileManager.default.fileExists(
                    atPath: chunkDirectory.appendingPathComponent(chunk.fileName).path
                ))
            }
        )
        let expectedChunkCount = Int(ceil(Double(data.count) / Double(chunkSize)))

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.last)
        let lastUploadedChunkNameInt = try XCTUnwrap(Int(lastUploadedChunk.fileName))
        XCTAssertEqual(firstUploadedChunkNameInt, 1)
        XCTAssertEqual(lastUploadedChunkNameInt, expectedChunkCount)
        XCTAssertEqual(Int(firstUploadedChunk.size), chunkSize)
        XCTAssertEqual(
            Int(lastUploadedChunk.size), data.count - ((lastUploadedChunkNameInt - 1) * chunkSize)
        )
        XCTAssertTrue(uploadedChunks.chunksExisted)
        XCTAssertFalse(FileManager.default.fileExists(atPath: chunkDirectory.path))
        XCTAssertEqual(
            Self.dbManager.ncDatabase().objects(RemoteFileChunk.self)
                .where {
                    $0.remoteChunkStoreFolderName.starts(
                        with: chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)
                    )
                }
                .count,
            0
        )
    }

    func testFailedChunkedUploadKeepsTemporaryChunksForResume() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        remoteInterface.uploadError = NKError(statusCode: 500, fallbackDescription: "Upload failed")
        remoteInterface.chunkUploadCompletedChunkCount = 1

        let chunkSize = 3
        let itemIdentifier = "failed-chunked-upload-\(UUID().uuidString)"
        let chunkDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("mock-chunks-\(UUID().uuidString)", isDirectory: true)
        remoteInterface.chunkUploadDirectory = chunkDirectory
        defer { try? FileManager.default.removeItem(at: chunkDirectory) }

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError.errorCode, 500)
        XCTAssertTrue(FileManager.default.fileExists(atPath: chunkDirectory.path))
        let storedChunks = try FileManager.default.contentsOfDirectory(atPath: chunkDirectory.path)
        XCTAssertEqual(storedChunks.count, Int(ceil(Double(data.count) / Double(chunkSize))))
        XCTAssertEqual(
            Self.dbManager.ncDatabase().objects(RemoteFileChunk.self)
                .where {
                    $0.remoteChunkStoreFolderName.starts(
                        with: chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)
                    )
                }
                .count,
            2
        )
    }

    func testFailedChunkedUploadWithoutRemainingChunksRemovesTemporaryDirectory() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }

        let remoteInterface = MockRemoteInterface(
            account: Self.account,
            rootItem: MockRemoteItem.rootItem(account: Self.account)
        )
        remoteInterface.uploadError = NKError(statusCode: 500, fallbackDescription: "Upload failed")

        let itemIdentifier = "failed-complete-chunks-\(UUID().uuidString)"
        let chunkDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("mock-chunks-\(UUID().uuidString)", isDirectory: true)
        remoteInterface.chunkUploadDirectory = chunkDirectory
        defer { try? FileManager.default.removeItem(at: chunkDirectory) }

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: 3,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError.errorCode, 500)
        XCTAssertFalse(FileManager.default.fileExists(atPath: chunkDirectory.path))
        XCTAssertEqual(
            Self.dbManager.ncDatabase().objects(RemoteFileChunk.self)
                .where {
                    $0.remoteChunkStoreFolderName.starts(
                        with: chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)
                    )
                }
                .count,
            0
        )
    }

    func testResumingInterruptedChunkedUpload() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let chunkSize = 3

        // The chunk id is derived from (item, size, modificationDate). Seed the prior attempt's
        // bookkeeping under exactly that derived id so the resume path recognises identical content.
        let itemIdentifier = "resume-item"
        let modificationDate = Date(timeIntervalSince1970: 1_700_000_000)
        let uploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: Int64(data.count),
            modificationDate: modificationDate
        )

        let previousUploadedChunkNum = 1
        let previousUploadedChunk = RemoteFileChunk(
            fileName: String(previousUploadedChunkNum),
            size: Int64(chunkSize),
            remoteChunkStoreFolderName: uploadId
        )
        remoteInterface.currentChunks = [uploadId: [previousUploadedChunk]]

        let db = Self.dbManager.ncDatabase()
        try db.write {
            db.add([
                RemoteFileChunk(
                    fileName: String(previousUploadedChunkNum + 1),
                    size: Int64(chunkSize),
                    remoteChunkStoreFolderName: uploadId
                ),
                RemoteFileChunk(
                    fileName: String(previousUploadedChunkNum + 2),
                    size: Int64(data.count - (chunkSize * (previousUploadedChunkNum + 1))),
                    remoteChunkStoreFolderName: uploadId
                )
            ])
        }

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            modificationDate: modificationDate,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.record($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        // Only the not-yet-uploaded chunks (2 and 3) are re-sent; chunk 1 is resumed from the server.
        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.last)
        let lastUploadedChunkNameInt = try XCTUnwrap(Int(lastUploadedChunk.fileName))
        XCTAssertEqual(firstUploadedChunkNameInt, previousUploadedChunkNum + 1)
        XCTAssertEqual(lastUploadedChunkNameInt, previousUploadedChunkNum + 2)
        XCTAssertEqual(Int(firstUploadedChunk.size), chunkSize)
        XCTAssertEqual(
            Int(lastUploadedChunk.size), data.count - ((lastUploadedChunkNameInt - 1) * chunkSize)
        )
    }

    func testUsingServerCapabilitiesChunkSize() async throws {
        let capabilities = ##"""
        {
            "ocs": {
                "meta": {
                    "status": "ok",
                    "statuscode": 100,
                    "message": "OK",
                    "totalitems": "",
                    "itemsperpage": ""
                },
                "data": {
                    "version": {
                        "major": 28,
                        "minor": 0,
                        "micro": 4,
                        "string": "28.0.4",
                        "edition": "",
                        "extendedSupport": false
                    },
                    "capabilities": {
                        "core": {
                            "pollinterval": 60,
                            "webdav-root": "remote.php/webdav",
                            "reference-api": true,
                            "reference-regex": "(\\s|\n|^)(https?:\\/\\/)((?:[-A-Z0-9+_]+\\.)+[-A-Z]+(?:\\/[-A-Z0-9+&@#%?=~_|!:,.;()]*)*)(\\s|\n|$)"
                        },
                        "files": {
                            "bigfilechunking": true,
                            "blacklisted_files": [
                                ".htaccess"
                            ],
                            "chunked_upload": {
                                "max_size": 4,
                                "max_parallel_count": 5
                            },
                            "directEditing": {
                                "url": "https://mock.nc.com/ocs/v2.php/apps/files/api/v1/directEditing",
                                "etag": "c748e8fc588b54fc5af38c4481a19d20",
                                "supportsFileId": true
                            },
                            "comments": true,
                            "undelete": true,
                            "versioning": true,
                            "version_labeling": true,
                            "version_deletion": true
                        },
                        "dav": {
                            "chunking": "1.0",
                            "bulkupload": "1.0"
                        }
                    }
                }
            }
        }
        """##
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        remoteInterface.capabilities = capabilities

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            forItemWithIdentifier: "caps-chunk-size-item",
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.record($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        XCTAssertEqual(uploadedChunks.chunks.first?.size, 4)
        XCTAssertEqual(uploadedChunks.chunks.last?.size, 4)
    }

    func testUsingServerCapabilitiesWithoutChunkSize() async throws {
        let capabilities = ##"""
        {
            "ocs": {
                "meta": {
                    "status": "ok",
                    "statuscode": 100,
                    "message": "OK",
                    "totalitems": "",
                    "itemsperpage": ""
                },
                "data": {
                    "version": {
                        "major": 28,
                        "minor": 0,
                        "micro": 4,
                        "string": "28.0.4",
                        "edition": "",
                        "extendedSupport": false
                    },
                    "capabilities": {
                        "core": {
                            "pollinterval": 60,
                            "webdav-root": "remote.php/webdav",
                            "reference-api": true,
                            "reference-regex": "(\\s|\n|^)(https?:\\/\\/)((?:[-A-Z0-9+_]+\\.)+[-A-Z]+(?:\\/[-A-Z0-9+&@#%?=~_|!:,.;()]*)*)(\\s|\n|$)"
                        },
                        "files": {
                            "bigfilechunking": true,
                            "blacklisted_files": [
                                ".htaccess"
                            ],
                            "comments": true,
                            "undelete": true,
                            "versioning": true,
                            "version_labeling": true,
                            "version_deletion": true
                        },
                        "dav": {
                            "chunking": "1.0",
                            "bulkupload": "1.0"
                        }
                    }
                }
            }
        }
        """##
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: defaultFileChunkSize + 1)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        remoteInterface.capabilities = capabilities

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            forItemWithIdentifier: "caps-no-chunk-size-item",
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.record($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        XCTAssertEqual(uploadedChunks.chunks.first?.size, Int64(defaultFileChunkSize))
        XCTAssertEqual(uploadedChunks.chunks.last?.size, 1)
    }

    /// F3 content-safety: a prior interrupted chunked upload of a *different* version of the same item
    /// (different derived id) must NOT be resumed. Its stale chunk bookkeeping is swept and the upload
    /// starts fresh, so old chunks can never be spliced into the new content.
    func testChunkedUploadDiscardsStaleChunksAfterContentChange() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let chunkSize = 3
        let itemIdentifier = "content-change-item"

        // Seed an older attempt whose individual chunk rows have already been consumed.
        let staleModificationDate = Date(timeIntervalSince1970: 1_600_000_000)
        let staleUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: Int64(data.count),
            modificationDate: staleModificationDate
        )
        let staleChunksDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("stale-chunks-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(
            at: staleChunksDirectory,
            withIntermediateDirectories: true
        )
        try Data([1]).write(to: staleChunksDirectory.appendingPathComponent("2"))
        remoteInterface.chunkUploadDirectories[staleUploadId] = staleChunksDirectory
        defer { try? FileManager.default.removeItem(at: staleChunksDirectory) }

        var metadata = SendableItemMetadata(
            ocId: itemIdentifier,
            fileName: "file.txt",
            account: Self.account
        )
        metadata.status = Status.uploadError.rawValue
        metadata.chunkUploadId = staleUploadId
        Self.dbManager.addItemMetadata(metadata)

        // The newer version (different mtime) derives a different id.
        let newModificationDate = Date(timeIntervalSince1970: 1_700_000_000)
        let newUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: Int64(data.count),
            modificationDate: newModificationDate
        )
        XCTAssertNotEqual(staleUploadId, newUploadId)

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            modificationDate: newModificationDate,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.record($0) }
        )

        XCTAssertEqual(result.remoteError, .success)

        // The old metadata pointer is consumed before it is replaced with the new attempt.
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: itemIdentifier)?.chunkUploadId)
        XCTAssertFalse(FileManager.default.fileExists(atPath: staleChunksDirectory.path))

        // The upload started fresh (chunk 1 was re-sent, not resumed from chunk 2).
        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.first)
        XCTAssertEqual(Int(firstUploadedChunk.fileName), 1)
    }
}
