//
//  ItemDeleteTests.swift
//
//
//  Created by Claudio Cambra on 13/5/24.
//

import FileProvider
import NextcloudKit
import RealmSwift
import TestInterface
import XCTest
@testable import NextcloudFileProviderKit

final class ItemDeleteTests: XCTestCase {
    static let account = Account(
        user: "testUser", serverUrl: "https://mock.nc.com", password: "abcd"
    )
    static let rootItem = MockRemoteItem(
        identifier: NSFileProviderItemIdentifier.rootContainer.rawValue,
        name: "root",
        directory: true
    )
    static let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    override func tearDown() {
        Self.rootItem.children = []
    }

    func testDeleteFile() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: Self.rootItem)
        let itemIdentifier = "file"
        let remoteItem = MockRemoteItem(identifier: itemIdentifier, name: "file")
        remoteItem.parent = Self.rootItem
        Self.rootItem.children = [remoteItem]

        XCTAssertFalse(Self.rootItem.children.isEmpty)

        let itemMetadata = ItemMetadata()
        itemMetadata.ocId = itemIdentifier
        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            remoteInterface: remoteInterface
        )

        let (error) = await item.delete(dbManager: Self.dbManager)
        XCTAssertNil(error)
        XCTAssertTrue(Self.rootItem.children.isEmpty)
    }
}
