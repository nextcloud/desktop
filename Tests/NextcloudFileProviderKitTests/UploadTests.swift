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

    func testSucceededUploadResult() {
        let uploadResult = UploadResult(
            ocId: nil,
            chunks: nil,
            etag: nil,
            date: nil,
            size: nil,
            afError: nil,
            remoteError: .success
        )
        XCTAssertTrue(uploadResult.succeeded)
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

        XCTAssertTrue(result.succeeded)
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
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: account,
            inChunksSized: 3,
            dbManager: dbManager
        )

        XCTAssertTrue(result.succeeded)
        XCTAssertNotNil(result.chunks)
        XCTAssertEqual(result.chunks?.count, 3)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)
    }
}
