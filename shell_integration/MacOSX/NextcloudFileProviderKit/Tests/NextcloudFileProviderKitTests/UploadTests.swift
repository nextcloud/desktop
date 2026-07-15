//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import TestInterface
import XCTest

final class UploadTests: NextcloudFileProviderKitTestCase {
    static let account = Account(user: "user", id: "id", serverUrl: "test.cloud.com", password: "1234")
    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
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

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let chunkSize = 3
        var uploadedChunks = [RemoteFileChunk]()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: "chunked-upload-item",
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )
        let expectedChunkCount = Int(ceil(Double(data.count) / Double(chunkSize)))

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.last)
        let lastUploadedChunkNameInt = try XCTUnwrap(Int(lastUploadedChunk.fileName))
        XCTAssertEqual(firstUploadedChunkNameInt, 1)
        XCTAssertEqual(lastUploadedChunkNameInt, expectedChunkCount)
        XCTAssertEqual(Int(firstUploadedChunk.size), chunkSize)
        XCTAssertEqual(
            Int(lastUploadedChunk.size), data.count - ((lastUploadedChunkNameInt - 1) * chunkSize)
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
        var uploadedChunks = [RemoteFileChunk]()
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
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        // Only the not-yet-uploaded chunks (2 and 3) are re-sent; chunk 1 is resumed from the server.
        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.last)
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
        var uploadedChunks = [RemoteFileChunk]()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            forItemWithIdentifier: "caps-chunk-size-item",
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        XCTAssertEqual(uploadedChunks.first?.size, 4)
        XCTAssertEqual(uploadedChunks.last?.size, 4)
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
        var uploadedChunks = [RemoteFileChunk]()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            forItemWithIdentifier: "caps-no-chunk-size-item",
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        XCTAssertEqual(uploadedChunks.first?.size, Int64(defaultFileChunkSize))
        XCTAssertEqual(uploadedChunks.last?.size, 1)
    }

    /// F3 content-safety: a prior interrupted chunked upload of a *different* version of the same item
    /// (different derived id) must NOT be resumed. Its stale chunk bookkeeping is swept and the upload
    /// starts fresh, so old chunks can never be spliced into the new content.
    func testChunkedUploadDiscardsStaleChunksAfterContentChange() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let chunkSize = 3
        let itemIdentifier = "content-change-item"

        // Seed bookkeeping from a prior interrupted upload of an older version (older mtime).
        let staleModificationDate = Date(timeIntervalSince1970: 1_600_000_000)
        let staleUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: Int64(data.count),
            modificationDate: staleModificationDate
        )
        let db = Self.dbManager.ncDatabase()
        try db.write {
            db.add([
                RemoteFileChunk(fileName: "2", size: Int64(chunkSize), remoteChunkStoreFolderName: staleUploadId),
                RemoteFileChunk(fileName: "3", size: 2, remoteChunkStoreFolderName: staleUploadId)
            ])
        }

        // The newer version (different mtime) derives a different id.
        let newModificationDate = Date(timeIntervalSince1970: 1_700_000_000)
        let newUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: Int64(data.count),
            modificationDate: newModificationDate
        )
        XCTAssertNotEqual(staleUploadId, newUploadId)

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        var uploadedChunks = [RemoteFileChunk]()
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
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )

        XCTAssertEqual(result.remoteError, .success)

        // Stale rows for the previous version must have been swept.
        let dbAfterUpload = Self.dbManager.ncDatabase()
        let remainingStale = dbAfterUpload.objects(RemoteFileChunk.self)
            .where { $0.remoteChunkStoreFolderName == staleUploadId }
        XCTAssertEqual(remainingStale.count, 0)

        // The upload started fresh (chunk 1 was re-sent, not resumed from chunk 2).
        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.first)
        XCTAssertEqual(Int(firstUploadedChunk.fileName), 1)
    }
}
