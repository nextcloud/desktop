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
        let uploadUuid = UUID().uuidString
        let previousUploadedChunkNum = 1
        let previousUploadedChunk = RemoteFileChunk(
            fileName: String(previousUploadedChunkNum),
            size: Int64(chunkSize),
            remoteChunkStoreFolderName: uploadUuid
        )
        let previousUploadedChunks = [previousUploadedChunk]
        remoteInterface.currentChunks = [uploadUuid: previousUploadedChunks]

        let db = Self.dbManager.ncDatabase()
        try db.write {
            db.add([
                RemoteFileChunk(
                    fileName: String(previousUploadedChunkNum + 1),
                    size: Int64(chunkSize),
                    remoteChunkStoreFolderName: uploadUuid
                ),
                RemoteFileChunk(
                    fileName: String(previousUploadedChunkNum + 2),
                    size: Int64(data.count - (chunkSize * (previousUploadedChunkNum + 1))),
                    remoteChunkStoreFolderName: uploadUuid
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
            usingChunkUploadId: uploadUuid,
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.last)
        let lastUploadedChunkNameInt = try XCTUnwrap(Int(lastUploadedChunk.fileName))
        XCTAssertEqual(firstUploadedChunkNameInt, previousUploadedChunkNum + 1)
        XCTAssertEqual(lastUploadedChunkNameInt, previousUploadedChunkNum + 2)
        print(uploadedChunks)
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
}
