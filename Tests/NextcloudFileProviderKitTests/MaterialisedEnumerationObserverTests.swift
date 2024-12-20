//
//  MaterialisedEnumerationObserverTests.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-20.
//

import FileProvider
import Foundation
import NextcloudKit
import NextcloudFileProviderKit
import RealmSwift
import TestInterface
import XCTest

final class MaterialisedEnumerationObserverTests: XCTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    func testMaterialisedObserver() async {
        let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)
        let remoteInterface = MockRemoteInterface()
        let expect = XCTestExpectation(description: "Enumerator")
        let observer = MaterialisedEnumerationObserver(
            ncKitAccount: Self.account.ncKitAccount, dbManager: dbManager
        ) { deletedOcIds in
            XCTAssertTrue(deletedOcIds.isEmpty)
            expect.fulfill()
        }
        let enumerator = MockEnumerator(
            account: Self.account, dbManager: dbManager, remoteInterface: remoteInterface
        )
        enumerator.enumerateItems(for: observer, startingAt: NSFileProviderPage(Data(count: 1)))
        await fulfillment(of: [expect], timeout: 1)
    }

    func testMaterialisedFiles() async {
        let itemA = ItemMetadata()
        itemA.apply(account: Self.account)
        itemA.apply(fileName: "itemA")
        itemA.ocId = "itemA"
        itemA.serverUrl = Self.account.davFilesUrl

        let itemB = ItemMetadata()
        itemB.apply(account: Self.account)
        itemB.apply(fileName: "itemB")
        itemB.ocId = "itemB"
        itemB.serverUrl = Self.account.davFilesUrl

        let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)
        dbManager.addItemMetadata(itemA)
        dbManager.addItemMetadata(itemB)

        let remoteInterface = MockRemoteInterface()
        let expect = XCTestExpectation(description: "Enumerator")
        let observer = MaterialisedEnumerationObserver(
            ncKitAccount: Self.account.ncKitAccount, dbManager: dbManager
        ) { deletedOcIds in
            XCTAssertEqual(deletedOcIds.count, 1) // Item B deleted
            XCTAssertEqual(deletedOcIds.first, itemB.ocId)
            expect.fulfill()
        }
        let enumerator = MockEnumerator(
            account: Self.account, dbManager: dbManager, remoteInterface: remoteInterface
        )
        enumerator.enumeratorItems = [itemA] // No item B
        enumerator.enumerateItems(for: observer, startingAt: NSFileProviderPage(Data(count: 1)))
        await fulfillment(of: [expect], timeout: 1)
    }
}
