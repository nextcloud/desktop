//
//  UploadTests.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2025-01-07.
//

import RealmSwift
import TestInterface
import XCTest
@testable import NextcloudFileProviderKit

final class UploadTests: XCTestCase {
    let account = Account(user: "user", id: "id", serverUrl: "test.cloud.com", password: "1234")
    let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

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
            MockRemoteInterface(rootItem: MockRemoteItem.rootItem(account: account))
        let remotePath = account.davFilesUrl + "/file.txt"
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: account,
            dbManager: dbManager
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertNil(result.chunks)
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
            MockRemoteInterface(rootItem: MockRemoteItem.rootItem(account: account))
        let remotePath = account.davFilesUrl + "/file.txt"
        let chunkSize = 3
        var uploadedChunks = [RemoteFileChunk]()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: account,
            inChunksSized: chunkSize,
            dbManager: dbManager,
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )
        let resultChunks = try XCTUnwrap(result.chunks)
        let expectedChunkCount = Int(ceil(Double(data.count) / Double(chunkSize)))

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(resultChunks.count, expectedChunkCount)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        XCTAssertEqual(uploadedChunks.count, resultChunks.count)

        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.last)
        let lastUploadedChunkNameInt = try XCTUnwrap(Int(lastUploadedChunk.fileName))
        XCTAssertEqual(firstUploadedChunkNameInt, 1)
        XCTAssertEqual(lastUploadedChunkNameInt, expectedChunkCount)
        XCTAssertEqual(Int(firstUploadedChunk.size), chunkSize)
        XCTAssertEqual(
            Int(lastUploadedChunk.size), data.count - (lastUploadedChunkNameInt * chunkSize)
        )
    }

    func testResumingInterruptedChunkedUpload() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(rootItem: MockRemoteItem.rootItem(account: account))
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

        let remotePath = account.davFilesUrl + "/file.txt"
        var uploadedChunks = [RemoteFileChunk]()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: account,
            inChunksSized: chunkSize,
            usingChunkUploadId: uploadUuid,
            dbManager: dbManager,
            chunkUploadCompleteHandler: { uploadedChunks.append($0) }
        )
        let resultChunks = try XCTUnwrap(result.chunks)
        let expectedChunkCount = Int(ceil(Double(data.count) / Double(chunkSize)))

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(resultChunks.count, expectedChunkCount)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        XCTAssertEqual(uploadedChunks.count, resultChunks.count - previousUploadedChunks.count)

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
}
