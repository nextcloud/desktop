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

    func testMaterialisedFiles() async {
        let dbManager = FilesDatabaseManager(realmConfig: .defaultConfiguration)
        let remoteInterface = MockRemoteInterface()
        let expect = XCTestExpectation(description: "Enumerator")
        let observer = MaterialisedEnumerationObserver(
            ncKitAccount: Self.account.ncKitAccount, dbManager: dbManager
        ) { deletedOcIds in
            XCTAssertEqual(deletedOcIds, [])
            expect.fulfill()
        }
        let enumerator = MockEnumerator(
            account: Self.account, dbManager: dbManager, remoteInterface: remoteInterface
        )
        enumerator.enumerateItems(for: observer, startingAt: NSFileProviderPage(Data(count: 1)))
        await fulfillment(of: [expect], timeout: 1)
    }
}
