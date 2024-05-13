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
}
