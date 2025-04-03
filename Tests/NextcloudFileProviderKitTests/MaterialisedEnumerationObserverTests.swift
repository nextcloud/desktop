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
        let dbManager = FilesDatabaseManager(
            realmConfig: .defaultConfiguration, account: Self.account.ncKitAccount
        )
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
        var itemA = SendableItemMetadata(ocId: "itemA", fileName: "itemA", account: Self.account)
        itemA.downloaded = true

        var itemB = SendableItemMetadata(ocId: "itemB", fileName: "itemB", account: Self.account)
        itemB.downloaded = false

        var itemC = SendableItemMetadata(ocId: "itemC", fileName: "itemC", account: Self.account)
        itemC.downloaded = true

        let dbManager = FilesDatabaseManager(
            realmConfig: .defaultConfiguration, account: Self.account.ncKitAccount
        )
        dbManager.addItemMetadata(itemA)
        dbManager.addItemMetadata(itemB)
        dbManager.addItemMetadata(itemC)

        let remoteInterface = MockRemoteInterface()
        let expect = XCTestExpectation(description: "Enumerator")
        let observer = MaterialisedEnumerationObserver(
            ncKitAccount: Self.account.ncKitAccount, dbManager: dbManager
        ) { deletedOcIds in
            // itemA is downloaded, itemB is not, itemC is (and is new). Only the local copy of
            // itemA should come up as deleted
            XCTAssertEqual(deletedOcIds.count, 1) // Item B deleted
            XCTAssertEqual(deletedOcIds.first, itemA.ocId)
            expect.fulfill()
        }
        let enumerator = MockEnumerator(
            account: Self.account, dbManager: dbManager, remoteInterface: remoteInterface
        )
        enumerator.enumeratorItems = [itemB, itemC]
        enumerator.enumerateItems(for: observer, startingAt: NSFileProviderPage(Data(count: 1)))
        await fulfillment(of: [expect], timeout: 1)
    }
}
