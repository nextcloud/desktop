//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
@testable import TestInterface
import XCTest

///
/// Baseline performance measurements for the first-open container enumeration path.
///
/// These tests use `MockRemoteInterface` and an in-memory Realm so they measure only the
/// extension-side work (PROPFIND response handling, metadata conversion, Realm persistence,
/// observer reporting). They do not exercise real network I/O.
///
/// The thresholds are intentionally generous: the primary purpose is to establish a reproducible
/// baseline before the batch-write optimization. After PR 2 the numbers are re-recorded in the
/// plan document and the assertions can be tightened.
///
final class EnumeratorPerformanceTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "perfUser", id: "perfUserId", serverUrl: "https://perf.nc.com", password: "abcd"
    )

    static let dbManager = FilesDatabaseManager(
        account: account,
        databaseDirectory: makeDatabaseDirectory(),
        fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("perf-test"),
        log: FileProviderLogMock()
    )

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    ///
    /// Build a remote tree with a single folder containing `childCount` files.
    ///
    private func makeFolderWithChildren(_ childCount: Int) -> (MockRemoteItem, MockRemoteItem) {
        let rootItem = MockRemoteItem.rootItem(account: Self.account)

        let folder = MockRemoteItem(
            identifier: "perf-folder-\(childCount)",
            versionIdentifier: "FOLDER_ETAG",
            name: "folder-\(childCount)",
            remotePath: Self.account.davFilesUrl + "/folder-\(childCount)",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        folder.parent = rootItem
        rootItem.children = [folder]

        var children: [MockRemoteItem] = []
        children.reserveCapacity(childCount)
        for i in 0 ..< childCount {
            let child = MockRemoteItem(
                identifier: "perf-child-\(childCount)-\(i)",
                versionIdentifier: "ETAG-\(i)",
                name: "file-\(i).txt",
                remotePath: folder.remotePath + "/file-\(i).txt",
                account: Self.account.ncKitAccount,
                username: Self.account.username,
                userId: Self.account.id,
                serverUrl: Self.account.serverUrl
            )
            child.parent = folder
            children.append(child)
        }
        folder.children = children

        return (rootItem, folder)
    }

    ///
    /// Enumerate the folder and return the wall-clock duration of the first-open path.
    ///
    private func measureFirstOpen(childCount: Int, pagination: Bool) async throws -> Double {
        let (rootItem, folder) = makeFolderWithChildren(childCount)
        let remoteInterface = MockRemoteInterface(
            account: Self.account,
            rootItem: rootItem,
            pagination: pagination
        )

        // Seed the folder metadata into the database; the enumerator expects to find the
        // container's metadata before it can enumerate its children.
        let folderMetadata = folder.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(folderMetadata)

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .init(folder.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            pageSize: 1000,
            log: FileProviderLogMock()
        )

        let observer = MockEnumerationObserver(enumerator: enumerator)

        let clock = ContinuousClock()
        let start = clock.now
        try await observer.enumerateItems()
        let elapsed = clock.now - start

        XCTAssertNil(observer.error, "Enumeration should complete without error.")
        // The existing MockRemoteInterface/test setup reports the container itself plus its
        // children, so the observer sees childCount + 1 items. The timing is what matters.
        XCTAssertEqual(observer.items.count, childCount + 1, "Observer should receive the container plus all children.")

        return elapsed.fpSeconds
    }

    func testFirstOpenBaseline10() async throws {
        let elapsed = try await measureFirstOpen(childCount: 10, pagination: true)
        XCTAssertLessThan(elapsed, 0.5, "10-item first open should be well under 500 ms.")
        print("Baseline 10 items: \(elapsed * 1000) ms")
    }

    func testFirstOpenBaseline200() async throws {
        let elapsed = try await measureFirstOpen(childCount: 200, pagination: true)
        XCTAssertLessThan(elapsed, 1.0, "200-item first open should be under 1 s.")
        print("Baseline 200 items: \(elapsed * 1000) ms")
    }

    func testFirstOpenBaseline2000() async throws {
        let elapsed = try await measureFirstOpen(childCount: 2000, pagination: true)
        XCTAssertLessThan(elapsed, 3.0, "2k-item first open should be under 3 s on a dev machine.")
        print("Baseline 2,000 items: \(elapsed * 1000) ms")
    }

    func testFirstOpenBaseline7000() async throws {
        let elapsed = try await measureFirstOpen(childCount: 7000, pagination: true)
        XCTAssertLessThan(elapsed, 10.0, "7k-item first open should be under 10 s on a dev machine.")
        print("Baseline 7,000 items: \(elapsed * 1000) ms")
    }

    ///
    /// Verify that the non-paginated server (< 31) path is already batched and stays within budget.
    ///
    func testFirstOpenBaseline2000Unpaginated() async throws {
        let elapsed = try await measureFirstOpen(childCount: 2000, pagination: false)
        XCTAssertLessThan(elapsed, 3.0, "2k-item unpaginated first open should be under 3 s.")
        print("Baseline 2,000 items (unpaginated): \(elapsed * 1000) ms")
    }
}
