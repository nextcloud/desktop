//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudCapabilitiesKit
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import TestInterface
import XCTest

@available(macOS 14.0, iOS 17.0, *)
final class RemoteChangeObserverEtagOptimizationTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "localhost", password: "abcd"
    )

    var dbManager: FilesDatabaseManager!
    var mockRemoteInterface: MockRemoteInterface!

    override func setUp() {
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        dbManager = FilesDatabaseManager(account: Self.account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
        mockRemoteInterface = MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
    }

    func testUnchangedDirectoryShouldNotBeEnumerated() async throws {
        // This test demonstrates the original issue where multiple working set checks
        // would repeatedly enumerate the same unchanged folders

        // 1. Setup: Create a materialized root directory and subdirectory
        var rootFolder = SendableItemMetadata(
            ocId: "root", fileName: "", account: Self.account
        )
        rootFolder.directory = true
        rootFolder.visitedDirectory = true
        rootFolder.etag = "rootetag123"
        rootFolder.serverUrl = Self.account.davFilesUrl
        dbManager.addItemMetadata(rootFolder)

        var customersFolder = SendableItemMetadata(
            ocId: "customers", fileName: "Customers", account: Self.account
        )
        customersFolder.directory = true
        customersFolder.visitedDirectory = true
        customersFolder.etag = "68662da77122d" // The etag from the logs
        dbManager.addItemMetadata(customersFolder)

        // Add some child files
        var childFile1 = SendableItemMetadata(
            ocId: "child1", fileName: "child1.txt", account: Self.account
        )
        childFile1.downloaded = true
        childFile1.serverUrl = Self.account.davFilesUrl + "/Customers"
        dbManager.addItemMetadata(childFile1)

        var childFile2 = SendableItemMetadata(
            ocId: "child2", fileName: "child2.txt", account: Self.account
        )
        childFile2.downloaded = true
        childFile2.serverUrl = Self.account.davFilesUrl + "/Customers"
        dbManager.addItemMetadata(childFile2)

        // 2. Setup server to return same etag (no changes)
        let serverCustomersFolder = MockRemoteItem(
            identifier: "customers",
            versionIdentifier: "68662da77122d", // Same etag - no changes
            name: "Customers",
            remotePath: Self.account.davFilesUrl + "/Customers",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )

        // Add the same children to server
        let serverChild1 = MockRemoteItem(
            identifier: "child1", name: "child1.txt",
            remotePath: serverCustomersFolder.remotePath + "/child1.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username, userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let serverChild2 = MockRemoteItem(
            identifier: "child2", name: "child2.txt",
            remotePath: serverCustomersFolder.remotePath + "/child2.txt",
            account: Self.account.ncKitAccount,
            username: Self.account.username, userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        serverCustomersFolder.children = [serverChild1, serverChild2]
        mockRemoteInterface.rootItem?.children = [serverCustomersFolder]

        // 3. Track how many times enumerate is called and for which paths
        var enumerateCallCount = 0
        var enumeratedPaths: [String] = []
        mockRemoteInterface.enumerateCallHandler = { remotePath, depth, _, _, _, _, _, _ in
            enumerateCallCount += 1
            enumeratedPaths.append(remotePath)
            print("ENUMERATE CALLED #\(enumerateCallCount) for: \(remotePath) (depth: \(depth))")
        }

        // 4. Create observer and manually trigger working set check
        let changeNotificationInterface = MockChangeNotificationInterface()
        let remoteChangeObserver = RemoteChangeObserver(
            account: Self.account,
            remoteInterface: mockRemoteInterface,
            changeNotificationInterface: changeNotificationInterface,
            domain: nil,
            dbManager: dbManager,
            log: FileProviderLogMock()
        )

        // 5. Debug: Check what materialized items we have
        let materializedItems = dbManager.materialisedItemMetadatas(account: Self.account.ncKitAccount)
        print("Materialized items found: \(materializedItems.count)")
        for item in materializedItems {
            print("  - \(item.fileName) (ocId: \(item.ocId), directory: \(item.directory), etag: \(item.etag))")
        }

        // 6. Simulate the original issue: multiple working set checks in quick succession
        // This would happen when multiple notify_file messages arrive or polling occurs frequently
        print("\n=== Running multiple working set checks ===")

        // First working set check
        let firstWorkingSetCheckCompleted = expectation(description: "First working set check completed.")

        remoteChangeObserver.startWorkingSetCheck {
            firstWorkingSetCheckCompleted.fulfill()
        }

        await fulfillment(of: [firstWorkingSetCheckCompleted])

        // Second working set check (simulating rapid notify_file messages)
        let secondWorkingSetCheckCompleted = expectation(description: "Second working set check completed.")

        remoteChangeObserver.startWorkingSetCheck {
            secondWorkingSetCheckCompleted.fulfill()
        }

        await fulfillment(of: [secondWorkingSetCheckCompleted])

        // Third working set check
        let thirdWorkingSetCheckCompleted = expectation(description: "Third working set check completed.")

        remoteChangeObserver.startWorkingSetCheck {
            thirdWorkingSetCheckCompleted.fulfill()
        }

        await fulfillment(of: [thirdWorkingSetCheckCompleted])

        // Wait for all operations to complete

        print("\n=== Results ===")
        print("Total enumerate calls: \(enumerateCallCount)")
        print("Enumerated paths: \(enumeratedPaths)")

        // 7. Assert: With the optimization, we should not make excessive enumerate calls
        // Each unique path should only be enumerated once or very few times
        XCTAssertGreaterThan(enumerateCallCount, 0, "At least one enumerate call should be made")

        // Count how many times the Customers folder was enumerated
        let customersEnumerateCount = enumeratedPaths.count(where: {
            $0.contains("Customers")
        })

        print("Customers folder enumerated \(customersEnumerateCount) times")

        // The key optimization we want: the same folder with unchanged etag shouldn't be
        // enumerated repeatedly. Ideally, it should be enumerated only once.
        // However, without optimization, it might be enumerated 3 times (once per working set check)
        XCTAssertLessThanOrEqual(customersEnumerateCount, 1, "Customers folder with unchanged etag should not be enumerated repeatedly")
    }
}
