//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
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
        dbManager = try! FilesDatabaseManager(
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
    /// The root container is rebuilt from scratch on every `item(for:)`, so dates taken from
    /// `Date()` re-queued its `update-item` job on every read.
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

    private func row(ocId: String, serverUrl: String, fileName: String) -> SendableItemMetadata {
        var metadata = SendableItemMetadata(ocId: ocId, fileName: fileName, account: Self.account)
        metadata.serverUrl = serverUrl
        return metadata
    }

    ///
    /// `childItemCount` must be the number of items *directly* in the container, or every folder
    /// holding subfolders disagrees with the framework's own count on every read.
    ///
    func testChildItemCountCountsDirectChildrenOnly() throws {
        var directory = row(ocId: "dir", serverUrl: "https://cloud.example.com/files", fileName: "docs")
        directory.directory = true
        var subdirectory = row(ocId: "subdirectory", serverUrl: "https://cloud.example.com/files/docs", fileName: "nested")
        subdirectory.directory = true
        var tombstone = row(ocId: "tombstone", serverUrl: "https://cloud.example.com/files/docs", fileName: "gone.txt")
        tombstone.deleted = true
        var otherAccountChild = row(ocId: "other-account-child", serverUrl: "https://cloud.example.com/files/docs", fileName: "theirs.txt")
        otherAccountChild.account = "someoneElse"

        for metadata in [
            directory,
            row(ocId: "direct-child", serverUrl: "https://cloud.example.com/files/docs", fileName: "report.pdf"),
            subdirectory,
            row(ocId: "grandchild", serverUrl: "https://cloud.example.com/files/docs/nested", fileName: "deep.txt"),
            tombstone,
            otherAccountChild
        ] {
            try dbManager.insertForTesting(metadata)
        }

        let count = dbManager.childItemCount(directoryMetadata: directory)

        XCTAssertEqual(count, 2, "Only the direct child and the subdirectory count; not the grandchild, the tombstone, or the other account's row.")
    }

    // MARK: - Materialized set reconciliation

    ///
    /// `fetchContents` persists `downloaded = true` before the system adds the file to its
    /// materialized set, so reconciling in that window flipped the row straight back to dataless.
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
    /// evicted, since `visitedDirectory` is preserved and marking it dataless never removes it
    /// from the candidate set.
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
    /// A failed enumeration is a partial result, so reconciling it marked everything the system had
    /// not yet reported as dataless.
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
