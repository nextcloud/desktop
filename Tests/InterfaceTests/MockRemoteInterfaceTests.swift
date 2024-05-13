//
//  MockRemoteInterfaceTests.swift
//
//
//  Created by Claudio Cambra on 13/5/24.
//

import XCTest
@testable import NextcloudFileProviderKit
@testable import TestInterface

final class MockRemoteInterfaceTests: XCTestCase {
    static let account = Account(
        user: "testUser", serverUrl: "https://mock.nc.com", password: "abcd"
    )
    static let rootItem = MockRemoteItem(
        identifier: "root",
        versionIdentifier: "root",
        name: "root",
        directory: true
    )

    override func tearDown() {
        Self.rootItem.children = []
    }

    func testItemForRemotePath() {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let itemA = MockRemoteItem(
            identifier: "a", versionIdentifier: "a", name: "a", directory: true
        )
        let itemB = MockRemoteItem(
            identifier: "b", versionIdentifier: "b", name: "b", directory: true
        )
        let itemA_B = MockRemoteItem(
            identifier: "b", versionIdentifier: "b", name: "b", directory: true
        )
        let targetItem = MockRemoteItem(
            identifier: "target", versionIdentifier: "target", name: "target"
        )

        remoteInterface.rootItem?.children = [itemA, itemB]
        itemA.parent = remoteInterface.rootItem
        itemB.parent = remoteInterface.rootItem
        itemA.children = [itemA_B]
        itemA_B.parent = itemA
        itemA_B.children = [targetItem]
        targetItem.parent = itemA_B

        XCTAssertEqual(remoteInterface.item(remotePath: "/a/b/target"), targetItem)
    }

    func testItemForRootPath() {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        XCTAssertEqual(remoteInterface.item(remotePath: "/"), Self.rootItem)
    }

    func testPathParentPath() {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let testPath = "/a/B/c/d"
        let expectedPath = "/a/B/c"

        XCTAssertEqual(remoteInterface.parentPath(path: testPath), expectedPath)
    }

    func testRootPathParentPath() {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let testPath = "/"
        let expectedPath = "/"

        XCTAssertEqual(remoteInterface.parentPath(path: testPath), expectedPath)
    }

    func testNameFromPath() throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let testPath = "/a/b/c/d"
        let expectedName = "d"
        let name = try remoteInterface.name(from: testPath)
        XCTAssertEqual(name, expectedName)
    }

    func testCreateFolder() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let newFolderAPath = "/A"
        let newFolderA_BPath = "/A/B/"

        let resultA = await remoteInterface.createFolder(remotePath: newFolderAPath)
        XCTAssertEqual(resultA.error, .success)

        let resultA_B = await remoteInterface.createFolder(remotePath: newFolderA_BPath)
        XCTAssertEqual(resultA_B.error, .success)

        let itemA = remoteInterface.item(remotePath: newFolderAPath)
        XCTAssertNotNil(itemA)
        XCTAssertEqual(itemA?.name, "A")
        XCTAssertTrue(itemA?.directory ?? false)

        let itemA_B = remoteInterface.item(remotePath: newFolderA_BPath)
        XCTAssertNotNil(itemA_B)
        XCTAssertEqual(itemA_B?.name, "B")
        XCTAssertTrue(itemA_B?.directory ?? false)
    }

    func testUpload() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let fileUrl = URL.temporaryDirectory.appendingPathComponent("file.txt", conformingTo: .text)
        let fileData = Data("Hello, World!".utf8)
        let fileSize = Int64(fileData.count)
        try fileData.write(to: fileUrl)

        let result = await remoteInterface.upload(remotePath: "/", localPath: fileUrl.path)
        XCTAssertEqual(result.remoteError, .success)

        let remoteItem = remoteInterface.item(remotePath: "/file.txt")
        XCTAssertNotNil(remoteItem)

        XCTAssertEqual(remoteItem?.name, "file.txt")
        XCTAssertEqual(remoteItem?.size, fileSize)
        XCTAssertEqual(remoteItem?.data, fileData)
        XCTAssertEqual(result.size, fileSize)
    }

    func testMove() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let itemA = MockRemoteItem(identifier: "a", name: "a", directory: true)
        let itemB = MockRemoteItem(identifier: "b", name: "b", directory: true)
        let itemC = MockRemoteItem(identifier: "c", name: "c", directory: true)
        let targetItem = MockRemoteItem(identifier: "target", name: "target")

        remoteInterface.rootItem?.children = [itemA, itemB]
        itemA.parent = remoteInterface.rootItem
        itemA.children = [itemC]
        itemC.parent = itemA
        itemB.parent = remoteInterface.rootItem

        itemC.children = [targetItem]
        targetItem.parent = itemC

        let result = await remoteInterface.move(
            remotePathSource: "/a/c/target", remotePathDestination: "/b/targetRenamed"
        )
        XCTAssertEqual(result.error, .success)
        XCTAssertEqual(itemB.children, [targetItem])
        XCTAssertEqual(itemC.children, [])
        XCTAssertEqual(targetItem.parent, itemB)
        XCTAssertEqual(targetItem.name, "targetRenamed")
    }
}
