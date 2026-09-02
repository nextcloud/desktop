//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
@testable import TestInterface
import XCTest

///
/// Regressions for the container `update-item` retry loop observed during bulk materialisation of
/// a "keep downloaded" tree: containers whose reported identity changed on every read, containers
/// reporting their whole subtree as their child count, and freshly downloaded files being
/// reconciled straight back to dataless.
///
final class ContainerRetryLoopTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    private var dbManager: FilesDatabaseManager!
    private var remoteInterface: MockRemoteInterface!

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
        dbManager = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
        remoteInterface = MockRemoteInterface(account: Self.account)
    }

    private func makeRootContainer() -> Item {
        Item.rootContainer(
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            remoteSupportsTrash: false,
            log: FileProviderLogMock()
        )
    }

    // MARK: - Synthesised container timestamps

    ///
    /// The root container is rebuilt from scratch on every `item(for:)`. When its dates came from
    /// `Date()` the framework saw `diffs:lastUsedDate|btime|mtime` on every read and re-queued the
    /// container's `update-item` job forever.
    ///
    func testRootContainerReportsStableDatesWithoutPersistedRow() {
        let first = makeRootContainer()
        let second = makeRootContainer()

        XCTAssertEqual(first.creationDate, second.creationDate)
        XCTAssertEqual(first.contentModificationDate, second.contentModificationDate)
        XCTAssertEqual(first.lastUsedDate, second.lastUsedDate)
    }

    func testRootContainerReportsPersistedDates() {
        let creationDate = Date(timeIntervalSince1970: 1_396_778_454)
        let modificationDate = Date(timeIntervalSince1970: 1_423_500_279)

        var stored = SendableItemMetadata(
            ocId: NSFileProviderItemIdentifier.rootContainer.rawValue,
            fileName: "/",
            account: Self.account
        )
        stored.directory = true
        stored.creationDate = creationDate
        stored.date = modificationDate
        stored.etag = "root-etag"
        dbManager.addItemMetadata(stored)

        let item = makeRootContainer()

        XCTAssertEqual(item.creationDate, creationDate)
        XCTAssertEqual(item.contentModificationDate, modificationDate)
        XCTAssertEqual(item.lastUsedDate, modificationDate)
        XCTAssertEqual(item.metadata.etag, "root-etag")

        // Still stable across reads now that a row exists.
        XCTAssertEqual(makeRootContainer().contentModificationDate, modificationDate)
    }

    // MARK: - Child item count

    ///
    /// `childItemCount` must be the number of items *directly* in the container. Counting
    /// descendants made every folder holding subfolders disagree with the framework's own count on
    /// every read, and gave Finder wrong numbers.
    ///
    func testChildItemCountCountsDirectChildrenOnly() throws {
        let directory = RealmItemMetadata()
        directory.ocId = "dir"
        directory.account = Self.account.ncKitAccount
        directory.updateLocation(serverUrl: "https://cloud.example.com/files", fileName: "docs")
        directory.directory = true

        let directChild = RealmItemMetadata()
        directChild.ocId = "direct-child"
        directChild.account = Self.account.ncKitAccount
        directChild.updateLocation(serverUrl: "https://cloud.example.com/files/docs", fileName: "report.pdf")

        let subdirectory = RealmItemMetadata()
        subdirectory.ocId = "subdirectory"
        subdirectory.account = Self.account.ncKitAccount
        subdirectory.updateLocation(serverUrl: "https://cloud.example.com/files/docs", fileName: "nested")
        subdirectory.directory = true

        let grandchild = RealmItemMetadata()
        grandchild.ocId = "grandchild"
        grandchild.account = Self.account.ncKitAccount
        grandchild.updateLocation(serverUrl: "https://cloud.example.com/files/docs/nested", fileName: "deep.txt")

        let tombstone = RealmItemMetadata()
        tombstone.ocId = "tombstone"
        tombstone.account = Self.account.ncKitAccount
        tombstone.updateLocation(serverUrl: "https://cloud.example.com/files/docs", fileName: "gone.txt")
        tombstone.deleted = true

        let otherAccountChild = RealmItemMetadata()
        otherAccountChild.ocId = "other-account-child"
        otherAccountChild.account = "someoneElse"
        otherAccountChild.updateLocation(serverUrl: "https://cloud.example.com/files/docs", fileName: "theirs.txt")

        let realm = dbManager.ncDatabase()
        try realm.write {
            realm.add([directory, directChild, subdirectory, grandchild, tombstone, otherAccountChild])
        }

        let count = dbManager.childItemCount(directoryMetadata: SendableItemMetadata(value: directory))

        XCTAssertEqual(count, 2, "Only the direct child and the subdirectory count; not the grandchild, the tombstone, or the other account's row.")
    }

    // MARK: - Materialized set reconciliation

    ///
    /// `fetchContents` persists `downloaded = true` before the system adds the file to its
    /// materialized set. Reconciling in that window flipped the row back to dataless, the
    /// framework re-requested the content, and the cycle repeated — 132,165 dataless transitions
    /// in one observed extension log.
    ///
    func testRecentlyDownloadedItemIsNotReconciledAsEvicted() async {
        var downloaded = SendableItemMetadata(ocId: "fresh", fileName: "fresh.otf", account: Self.account)
        downloaded.downloaded = true
        dbManager.addItemMetadata(downloaded)

        PendingMaterializationRegistry.shared.recordDownloaded(NSFileProviderItemIdentifier("fresh"))

        let expectation = XCTestExpectation(description: "Reconciliation finished")
        let observer = MaterializedEnumerationObserver(
            account: Self.account, dbManager: dbManager, log: FileProviderLogMock()
        ) { _, evicted in
            XCTAssertFalse(
                evicted.contains(NSFileProviderItemIdentifier("fresh")),
                "A download the system has not confirmed yet must not count as evicted."
            )
            expectation.fulfill()
        }

        // The system reports nothing: it has not caught up with the download.
        observer.finishEnumerating(upTo: nil)

        await fulfillment(of: [expectation], timeout: 1)
        XCTAssertEqual(dbManager.itemMetadata(ocId: "fresh")?.downloaded, true)
    }

    ///
    /// A visited directory absent from the system's materialized set must not be reconciled as
    /// evicted. Only files carry materialized content; a directory qualifies as materialized
    /// through `visitedDirectory`, which the reconciliation preserves, so marking it dataless never
    /// removed it from the candidate set and it was re-evicted on every pass forever (630
    /// directories × 80 passes = 30,117 transitions in one seven-minute run).
    ///
    func testVisitedDirectoryIsNotReconciledAsEvicted() async {
        var directory = SendableItemMetadata(ocId: "folder", fileName: "glyphs", account: Self.account)
        directory.directory = true
        directory.visitedDirectory = true
        dbManager.addItemMetadata(directory)

        for pass in 1 ... 3 {
            let expectation = XCTestExpectation(description: "Reconciliation pass \(pass) finished")
            let observer = MaterializedEnumerationObserver(
                account: Self.account, dbManager: dbManager, log: FileProviderLogMock()
            ) { _, evicted in
                XCTAssertFalse(
                    evicted.contains(NSFileProviderItemIdentifier("folder")),
                    "A directory must never be reported as evicted (pass \(pass))."
                )
                expectation.fulfill()
            }

            observer.finishEnumerating(upTo: nil)
            await fulfillment(of: [expectation], timeout: 1)
        }

        let stored = dbManager.itemMetadata(ocId: "folder")
        XCTAssertEqual(stored?.visitedDirectory, true, "The refresh subscription must survive.")
    }

    ///
    /// The control for the test above: once the system has confirmed the item, an ordinary
    /// eviction must still be detected on the very next pass.
    ///
    func testConfirmedItemIsReconciledAsEvictedOnTheNextPass() async {
        var downloaded = SendableItemMetadata(ocId: "settled", fileName: "settled.otf", account: Self.account)
        downloaded.downloaded = true
        dbManager.addItemMetadata(downloaded)

        PendingMaterializationRegistry.shared.recordDownloaded(NSFileProviderItemIdentifier("settled"))

        // Pass one: the system reports the item, which clears the pending record.
        let confirmation = XCTestExpectation(description: "Confirmation pass finished")
        let confirmingObserver = MaterializedEnumerationObserver(
            account: Self.account, dbManager: dbManager, log: FileProviderLogMock()
        ) { _, _ in confirmation.fulfill() }
        confirmingObserver.didEnumerate([MockFileProviderItem(identifier: NSFileProviderItemIdentifier("settled"), filename: "settled.otf", isUploaded: true)])
        confirmingObserver.finishEnumerating(upTo: nil)
        await fulfillment(of: [confirmation], timeout: 1)

        // Pass two: the item is gone from the system's materialized set, i.e. genuinely evicted.
        let eviction = XCTestExpectation(description: "Eviction pass finished")
        let evictingObserver = MaterializedEnumerationObserver(
            account: Self.account, dbManager: dbManager, log: FileProviderLogMock()
        ) { _, evicted in
            XCTAssertTrue(evicted.contains(NSFileProviderItemIdentifier("settled")))
            eviction.fulfill()
        }
        evictingObserver.finishEnumerating(upTo: nil)
        await fulfillment(of: [eviction], timeout: 1)

        XCTAssertEqual(dbManager.itemMetadata(ocId: "settled")?.downloaded, false)
    }

    ///
    /// A failed enumeration is a partial result, not evidence of eviction. Reconciling it marked
    /// everything the system had not yet reported as dataless.
    ///
    func testFailedEnumerationDoesNotTouchTheDatabase() async {
        var downloaded = SendableItemMetadata(ocId: "untouched", fileName: "untouched.otf", account: Self.account)
        downloaded.downloaded = true
        dbManager.addItemMetadata(downloaded)

        let expectation = XCTestExpectation(description: "Error handling finished")
        let observer = MaterializedEnumerationObserver(
            account: Self.account, dbManager: dbManager, log: FileProviderLogMock()
        ) { materialized, evicted in
            XCTAssertTrue(materialized.isEmpty)
            XCTAssertTrue(evicted.isEmpty)
            expectation.fulfill()
        }

        observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))

        await fulfillment(of: [expectation], timeout: 1)
        XCTAssertEqual(
            dbManager.itemMetadata(ocId: "untouched")?.downloaded,
            true,
            "A partial enumeration must not mark unreported items dataless."
        )
    }
}
