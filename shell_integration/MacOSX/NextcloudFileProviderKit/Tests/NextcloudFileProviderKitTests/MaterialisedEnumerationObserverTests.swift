//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import RealmSwift
import TestInterface
import XCTest

final class MaterialisedEnumerationObserverTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    override func setUp() {
        super.setUp()
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
    }

    private var retainedDatabase: Realm?

    override func tearDown() {
        retainedDatabase = nil
        super.tearDown()
    }

    private func makeDatabaseManager() -> FilesDatabaseManager {
        let dbManager = FilesDatabaseManager(account: Self.account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
        retainedDatabase = dbManager.ncDatabase()
        return dbManager
    }

    private func downloadedFile(ocId: String) -> SendableItemMetadata {
        var metadata = SendableItemMetadata(ocId: ocId, fileName: "\(ocId).txt", account: Self.account)
        metadata.serverUrl = Self.account.davFilesUrl
        metadata.downloaded = true
        metadata.uploaded = true
        return metadata
    }

    private func materializedItem(_ metadata: SendableItemMetadata) -> MockFileProviderItem {
        MockFileProviderItem(identifier: .init(metadata.ocId), filename: metadata.fileName, isUploaded: metadata.uploaded)
    }

    /// A nonterminal page must not clear download flags for files returned on later pages.
    func testMaterialisedObserverWaitsForTerminalPageBeforeReconciling() throws {
        let dbManager = FilesDatabaseManager(account: Self.account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
        let database = dbManager.ncDatabase()
        defer { withExtendedLifetime(database) {} }
        var firstFile = SendableItemMetadata(ocId: "first-page", fileName: "first-page.txt", account: Self.account)
        firstFile.downloaded = true
        firstFile.uploaded = true
        var secondFile = SendableItemMetadata(ocId: "second-page", fileName: "second-page.txt", account: Self.account)
        secondFile.downloaded = true
        secondFile.uploaded = true
        var evictedFile = SendableItemMetadata(ocId: "evicted", fileName: "evicted.txt", account: Self.account)
        evictedFile.downloaded = true
        evictedFile.uploaded = true
        [firstFile, secondFile, evictedFile].forEach { dbManager.addItemMetadata($0) }
        var completionCount = 0
        var evictedIds = Set<NSFileProviderItemIdentifier>()
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { _, evicted in
            completionCount += 1
            evictedIds = evicted
        }
        let firstItem = MockFileProviderItem(identifier: .init(firstFile.ocId), filename: firstFile.fileName, isUploaded: true)
        let secondItem = MockFileProviderItem(identifier: .init(secondFile.ocId), filename: secondFile.fileName, isUploaded: true)

        observer.didEnumerate([firstItem])
        observer.finishEnumerating(upTo: NSFileProviderPage(Data("second-page".utf8)))

        XCTAssertEqual(completionCount, 0)
        XCTAssertTrue(dbManager.itemMetadata(ocId: secondFile.ocId)?.downloaded == true)
        XCTAssertTrue(dbManager.itemMetadata(ocId: evictedFile.ocId)?.downloaded == true)

        observer.didEnumerate([secondItem])
        observer.finishEnumerating(upTo: nil)

        XCTAssertEqual(completionCount, 1)
        XCTAssertEqual(evictedIds, [NSFileProviderItemIdentifier(evictedFile.ocId)])
        XCTAssertTrue(dbManager.itemMetadata(ocId: firstFile.ocId)?.downloaded == true)
        XCTAssertTrue(dbManager.itemMetadata(ocId: secondFile.ocId)?.downloaded == true)
        let storedEvictedFile = try XCTUnwrap(dbManager.itemMetadata(ocId: evictedFile.ocId))
        XCTAssertFalse(storedEvictedFile.downloaded)
        XCTAssertFalse(storedEvictedFile.deleted)
        XCTAssertTrue(storedEvictedFile.uploaded)
    }

    /// Requests every materialized-item page, including an empty middle page, before clearing download flags for absent files.
    func testMaterialisedObserverRequestsEveryContinuationPage() {
        let dbManager = makeDatabaseManager()
        let firstFile = downloadedFile(ocId: "first-page")
        let secondFile = downloadedFile(ocId: "second-page")
        let evictedFile = downloadedFile(ocId: "evicted")
        [firstFile, secondFile, evictedFile].forEach { dbManager.addItemMetadata($0) }
        let enumerator = MockEnumerator(account: Self.account, dbManager: dbManager, remoteInterface: MockRemoteInterface(account: Self.account))
        let initialPage = NSFileProviderPage(Data())
        let emptyPage = NSFileProviderPage(Data("empty-page".utf8))
        let finalPage = NSFileProviderPage(Data("final-page".utf8))
        var completionCount = 0
        var evictedIds = Set<NSFileProviderItemIdentifier>()
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { _, evicted in
            completionCount += 1
            evictedIds = evicted
        }
        enumerator.enumerateItemsHandler = { pageObserver, page in
            XCTAssertEqual(completionCount, 0)
            XCTAssertTrue(dbManager.itemMetadata(ocId: secondFile.ocId)?.downloaded == true)
            XCTAssertTrue(dbManager.itemMetadata(ocId: evictedFile.ocId)?.downloaded == true)
            switch page {
                case initialPage:
                    pageObserver.didEnumerate([self.materializedItem(firstFile)])
                    pageObserver.finishEnumerating(upTo: emptyPage)
                case emptyPage:
                    pageObserver.didEnumerate([])
                    pageObserver.finishEnumerating(upTo: finalPage)
                case finalPage:
                    pageObserver.didEnumerate([self.materializedItem(secondFile)])
                    pageObserver.finishEnumerating(upTo: nil)
                default:
                    XCTFail("Unexpected materialized enumeration page")
                    pageObserver.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
            }
        }

        observer.enumerateItems(using: enumerator)

        XCTAssertEqual(enumerator.enumeratedPages, [initialPage, emptyPage, finalPage])
        XCTAssertEqual(enumerator.invalidationCount, 1)
        XCTAssertEqual(completionCount, 1)
        XCTAssertEqual(evictedIds, [NSFileProviderItemIdentifier(evictedFile.ocId)])
        XCTAssertTrue(dbManager.itemMetadata(ocId: firstFile.ocId)?.downloaded == true)
        XCTAssertTrue(dbManager.itemMetadata(ocId: secondFile.ocId)?.downloaded == true)
        XCTAssertTrue(dbManager.itemMetadata(ocId: evictedFile.ocId)?.downloaded == false)
    }

    /// Retains the materialized-item enumerator across delayed page callbacks and releases it after the final page.
    func testMaterialisedObserverRetainsEnumeratorUntilLastPageFinishes() throws {
        let dbManager = makeDatabaseManager()
        let file = downloadedFile(ocId: "downloaded")
        dbManager.addItemMetadata(file)
        var enumerator: MockEnumerator? = MockEnumerator(account: Self.account, dbManager: dbManager, remoteInterface: MockRemoteInterface(account: Self.account))
        weak var retainedEnumerator = enumerator
        var pageObserver: (any NSFileProviderEnumerationObserver)?
        var requestedPages: [NSFileProviderPage] = []
        enumerator?.enumerateItemsHandler = { observer, page in
            pageObserver = observer
            requestedPages.append(page)
        }
        var completionCount = 0
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { materialized, evicted in
            completionCount += 1
            XCTAssertTrue(materialized.isEmpty)
            XCTAssertTrue(evicted.isEmpty)
        }
        try observer.enumerateItems(using: XCTUnwrap(enumerator))
        enumerator = nil

        XCTAssertNotNil(retainedEnumerator)
        XCTAssertEqual(completionCount, 0)
        pageObserver?.didEnumerate([materializedItem(file)])
        let finalPage = NSFileProviderPage(Data("final-page".utf8))
        pageObserver?.finishEnumerating(upTo: finalPage)
        XCTAssertEqual(requestedPages, [NSFileProviderPage(Data()), finalPage])
        XCTAssertNotNil(retainedEnumerator)
        XCTAssertEqual(completionCount, 0)

        pageObserver?.finishEnumerating(upTo: nil)

        XCTAssertNil(retainedEnumerator)
        XCTAssertEqual(completionCount, 1)
        XCTAssertTrue(dbManager.itemMetadata(ocId: file.ocId)?.downloaded == true)
    }

    /// A failed continuation page preserves download flags and completes materialized-item enumeration without reporting changes.
    func testMaterialisedObserverPreservesStateWhenContinuationPageFails() {
        let dbManager = makeDatabaseManager()
        let downloaded = downloadedFile(ocId: "downloaded")
        var dataless = downloadedFile(ocId: "dataless")
        dataless.downloaded = false
        [downloaded, dataless].forEach { dbManager.addItemMetadata($0) }
        let enumerator = MockEnumerator(account: Self.account, dbManager: dbManager, remoteInterface: MockRemoteInterface(account: Self.account))
        let initialPage = NSFileProviderPage(Data())
        let failingPage = NSFileProviderPage(Data("failing-page".utf8))
        var completionCount = 0
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { materialized, evicted in
            completionCount += 1
            XCTAssertTrue(materialized.isEmpty)
            XCTAssertTrue(evicted.isEmpty)
        }
        enumerator.enumerateItemsHandler = { pageObserver, page in
            if page == initialPage {
                pageObserver.didEnumerate([self.materializedItem(dataless)])
                pageObserver.finishEnumerating(upTo: failingPage)
            } else {
                XCTAssertEqual(page, failingPage)
                XCTAssertEqual(completionCount, 0)
                pageObserver.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
            }
        }

        observer.enumerateItems(using: enumerator)

        XCTAssertEqual(enumerator.enumeratedPages, [initialPage, failingPage])
        XCTAssertEqual(enumerator.invalidationCount, 1)
        XCTAssertEqual(completionCount, 1)
        XCTAssertTrue(dbManager.itemMetadata(ocId: downloaded.ocId)?.downloaded == true)
        XCTAssertTrue(dbManager.itemMetadata(ocId: dataless.ocId)?.downloaded == false)
    }

    /// Late page callbacks must neither change download flags nor complete materialized-item enumeration a second time.
    func testMaterialisedObserverIgnoresCallbacksAfterCompletion() {
        for fails in [false, true] {
            let dbManager = makeDatabaseManager()
            let downloaded = downloadedFile(ocId: "downloaded")
            var dataless = downloadedFile(ocId: "dataless")
            dataless.downloaded = false
            [downloaded, dataless].forEach { dbManager.addItemMetadata($0) }
            var completionCount = 0
            let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { _, _ in
                completionCount += 1
            }
            if fails {
                observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))
            } else {
                observer.finishEnumerating(upTo: nil)
            }

            observer.didEnumerate([materializedItem(downloaded), materializedItem(dataless)])
            observer.finishEnumerating(upTo: NSFileProviderPage(Data("late-page".utf8)))
            observer.finishEnumerating(upTo: nil)
            observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))

            XCTAssertEqual(completionCount, 1)
            XCTAssertEqual(dbManager.itemMetadata(ocId: downloaded.ocId)?.downloaded, fails)
            XCTAssertTrue(dbManager.itemMetadata(ocId: dataless.ocId)?.downloaded == false)
        }
    }

    func testFailedMaterializedEnumerationDoesNotEvictUnseenItems() {
        var existing = SendableItemMetadata(ocId: "existing", fileName: "existing.txt", account: Self.account)
        existing.downloaded = true
        let dbManager = FilesDatabaseManager(account: Self.account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
        dbManager.addItemMetadata(existing)

        let completion = expectation(description: "Failure completes without state changes")
        completion.assertForOverFulfill = true
        var completionCount = 0
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { materialized, evicted in
            completionCount += 1
            XCTAssertTrue(materialized.isEmpty)
            XCTAssertTrue(evicted.isEmpty)
            completion.fulfill()
        }
        observer.finishEnumeratingWithError(NSError(domain: "EnumerationTest", code: 1))
        XCTAssertEqual(dbManager.itemMetadata(ocId: "existing")?.downloaded, true)
        wait(for: [completion], timeout: 1)
        XCTAssertEqual(completionCount, 1)
    }

    func testPartialMaterializedEnumerationFailurePreservesMetadata() {
        var unseen = SendableItemMetadata(ocId: "unseen", fileName: "unseen.txt", account: Self.account)
        unseen.downloaded = true
        var seen = SendableItemMetadata(ocId: "seen", fileName: "seen.txt", account: Self.account)
        seen.downloaded = true
        let newlyMaterialized = SendableItemMetadata(ocId: "new", fileName: "new.txt", account: Self.account)
        let dbManager = FilesDatabaseManager(account: Self.account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
        for metadata in [unseen, seen, newlyMaterialized] {
            dbManager.addItemMetadata(metadata)
        }

        let completion = expectation(description: "Partial failure completes without state changes")
        completion.assertForOverFulfill = true
        var completionCount = 0
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { materialized, evicted in
            completionCount += 1
            XCTAssertTrue(materialized.isEmpty)
            XCTAssertTrue(evicted.isEmpty)
            completion.fulfill()
        }
        let remoteInterface = MockRemoteInterface(account: Self.account)
        let items = [seen, newlyMaterialized].map {
            Item(metadata: $0, parentItemIdentifier: .rootContainer, account: Self.account, remoteInterface: remoteInterface, dbManager: dbManager, displayFileActions: false, remoteSupportsTrash: false, log: FileProviderLogMock())
        }
        observer.didEnumerate(items)
        observer.finishEnumeratingWithError(NSError(domain: "EnumerationTest", code: 1))

        wait(for: [completion], timeout: 1)
        XCTAssertEqual(completionCount, 1)
        XCTAssertEqual(dbManager.itemMetadata(ocId: "unseen")?.downloaded, true)
        XCTAssertEqual(dbManager.itemMetadata(ocId: "seen")?.downloaded, true)
        XCTAssertEqual(dbManager.itemMetadata(ocId: "new")?.downloaded, false)
    }

    func testMaterialisedObserverWithNoPreexistingState() async {
        let dbManager = FilesDatabaseManager(account: Self.account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
        // The database is intentionally left empty.

        let remoteInterface = MockRemoteInterface(account: Self.account)

        let enumeratedFile =
            SendableItemMetadata(ocId: "file1", fileName: "file1.txt", account: Self.account)
        var enumeratedDir =
            SendableItemMetadata(ocId: "dir1", fileName: "dir1", account: Self.account)
        enumeratedDir.directory = true

        let expect = XCTestExpectation(description: "Enumerator completion handler called")

        // The observer's logic requires metadata to exist in the DB to update it.
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { newlyMaterialisedIds, unmaterialisedIds in
            XCTAssertTrue(
                unmaterialisedIds.isEmpty,
                "Unmaterialised set should be empty when DB starts empty."
            )

            // The items are correctly identified as newly materialized because they weren't in the
            // DB's materialized list (which was empty).
            XCTAssertEqual(
                newlyMaterialisedIds.count,
                2,
                "Both enumerated items should be identified as newly materialised."
            )
            XCTAssertTrue(newlyMaterialisedIds.contains(NSFileProviderItemIdentifier("file1")))
            XCTAssertTrue(newlyMaterialisedIds.contains(NSFileProviderItemIdentifier("dir1")))

            // Verify that the database state is NOT updated
            let fileMetadata = dbManager.itemMetadata(ocId: "file1")
            XCTAssertNil(
                fileMetadata,
                "Metadata should NOT be in the DB, as the observer does not add missing items."
            )

            let dirMetadata = dbManager.itemMetadata(ocId: "dir1")
            XCTAssertNil(
                dirMetadata,
                "Metadata should NOT be in the DB, as the observer does not add missing items."
            )

            expect.fulfill()
        }

        let enumerator = MockEnumerator(
            account: Self.account, dbManager: dbManager, remoteInterface: remoteInterface
        )
        enumerator.enumeratorItems = [enumeratedFile, enumeratedDir]
        enumerator.enumerateItems(for: observer, startingAt: NSFileProviderPage(Data(count: 1)))

        await fulfillment(of: [expect], timeout: 1)
    }

    func testMaterialisedObserverWithMixedState() async {
        // Setup a DB with a mix of materialized and non-materialised items.
        var itemA = SendableItemMetadata(ocId: "itemA", fileName: "itemA", account: Self.account)
        itemA.downloaded = true // Was materialised

        var itemB = SendableItemMetadata(ocId: "itemB", fileName: "itemB", account: Self.account)
        itemB.downloaded = false // Was NOT materialised

        var itemC = SendableItemMetadata(ocId: "itemC", fileName: "itemC", account: Self.account)
        itemC.downloaded = true // Was materialised

        var dirD = SendableItemMetadata(ocId: "dirD", fileName: "dirD", account: Self.account)
        dirD.directory = true
        dirD.visitedDirectory = true // Was materialised

        let dbManager = FilesDatabaseManager(account: Self.account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
        dbManager.addItemMetadata(itemA)
        dbManager.addItemMetadata(itemB)
        dbManager.addItemMetadata(itemC)
        dbManager.addItemMetadata(dirD)

        let remoteInterface = MockRemoteInterface(account: Self.account)
        let expect = XCTestExpectation(description: "Enumerator completion handler called")
        let enumeratorItemsToReturn = [itemB, itemC]

        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { newlyMaterialisedIds, unmaterialisedIds in
            // Unmaterialised: itemA was materialized but not in the latest enumeration. dirD is
            // also absent from the enumeration, but keeps its visitedDirectory subscription and
            // has no other state to clear, so it must not be reported as a state transition.
            XCTAssertEqual(
                unmaterialisedIds.count, 1, "Only itemA should be reported as unmaterialised."
            )
            XCTAssertTrue(unmaterialisedIds.contains(NSFileProviderItemIdentifier("itemA")))

            // Newly Materialised: itemB was NOT materialized but WAS in the latest enumeration.
            XCTAssertEqual(
                newlyMaterialisedIds.count, 1, "itemB should be reported as newly materialised."
            )
            XCTAssertEqual(newlyMaterialisedIds.first, NSFileProviderItemIdentifier("itemB"))

            // Check final database state
            let finalItemA = dbManager.itemMetadata(ocId: "itemA")
            XCTAssertFalse(
                finalItemA?.downloaded ?? true, "itemA should now be marked as not downloaded."
            )

            let finalItemB = dbManager.itemMetadata(ocId: "itemB")
            XCTAssertTrue(
                finalItemB?.downloaded ?? false, "itemB should now be marked as downloaded."
            )

            let finalItemC = dbManager.itemMetadata(ocId: "itemC")
            XCTAssertTrue(finalItemC?.downloaded ?? false, "itemC should remain downloaded.")

            let finalDirD = dbManager.itemMetadata(ocId: "dirD")
            XCTAssertTrue(
                finalDirD?.visitedDirectory ?? false, "dirD should be marked as visited."
            )

            expect.fulfill()
        }

        let enumerator = MockEnumerator(
            account: Self.account, dbManager: dbManager, remoteInterface: remoteInterface
        )
        enumerator.enumeratorItems = enumeratorItemsToReturn
        enumerator.enumerateItems(for: observer, startingAt: NSFileProviderPage(Data(count: 1)))

        await fulfillment(of: [expect], timeout: 1)
    }

    ///
    /// Regression test for the non-converging reconciliation loop reported in
    /// [#10558](https://github.com/nextcloud/desktop/issues/10558).
    ///
    /// A directory which was browsed before keeps `visitedDirectory == true` even when the system
    /// reports it as dataless, because that flag subscribes it to remote change scanning. That
    /// also keeps it in the database's materialized item set, so it reappears as an eviction
    /// candidate on every reconciliation pass. The observer therefore must only persist and
    /// report actual state transitions — otherwise it re-marks the same directories as dataless
    /// on every pass and reconciliation never reaches a quiescent state.
    ///
    func testMaterialisedObserverConvergesForDatalessVisitedDirectories() async {
        // A directory which was browsed before but holds no materialized content any more.
        var visitedDir = SendableItemMetadata(ocId: "visitedDir", fileName: "visitedDir", account: Self.account)
        visitedDir.directory = true
        visitedDir.visitedDirectory = true
        visitedDir.downloaded = false

        // A directory which was browsed before and still carries a stale downloaded flag.
        var staleDir = SendableItemMetadata(ocId: "staleDir", fileName: "staleDir", account: Self.account)
        staleDir.directory = true
        staleDir.visitedDirectory = true
        staleDir.downloaded = true

        // A downloaded file about to be evicted.
        var file = SendableItemMetadata(ocId: "file", fileName: "file.txt", account: Self.account)
        file.downloaded = true

        let dbManager = FilesDatabaseManager(account: Self.account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
        dbManager.addItemMetadata(visitedDir)
        dbManager.addItemMetadata(staleDir)
        dbManager.addItemMetadata(file)

        let remoteInterface = MockRemoteInterface(account: Self.account)
        let firstPass = XCTestExpectation(description: "First pass completion handler called")

        // First pass: the system reports no materialized items at all.
        let firstObserver = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { newlyMaterialisedIds, unmaterialisedIds in
            XCTAssertTrue(
                newlyMaterialisedIds.isEmpty,
                "Nothing was enumerated, so nothing should be newly materialised."
            )

            // Only actual state transitions should be reported.
            XCTAssertEqual(
                unmaterialisedIds.count, 2, "The file and the stale directory should be reported as evicted."
            )
            XCTAssertTrue(unmaterialisedIds.contains(NSFileProviderItemIdentifier("file")))
            XCTAssertTrue(unmaterialisedIds.contains(NSFileProviderItemIdentifier("staleDir")))
            XCTAssertFalse(
                unmaterialisedIds.contains(NSFileProviderItemIdentifier("visitedDir")),
                "A visited directory in steady state must not be reported as a fresh eviction."
            )

            let persistedFile = dbManager.itemMetadata(ocId: "file")
            XCTAssertFalse(persistedFile?.downloaded ?? true, "The file should be marked as not downloaded.")

            let persistedStaleDir = dbManager.itemMetadata(ocId: "staleDir")
            XCTAssertFalse(persistedStaleDir?.downloaded ?? true, "The stale directory should lose its downloaded flag.")
            XCTAssertTrue(
                persistedStaleDir?.visitedDirectory ?? false,
                "The stale directory should keep its visitedDirectory subscription."
            )

            let persistedVisitedDir = dbManager.itemMetadata(ocId: "visitedDir")
            XCTAssertTrue(
                persistedVisitedDir?.visitedDirectory ?? false,
                "The visited directory should keep its visitedDirectory subscription."
            )

            firstPass.fulfill()
        }

        let firstEnumerator = MockEnumerator(
            account: Self.account, dbManager: dbManager, remoteInterface: remoteInterface
        )
        firstEnumerator.enumeratorItems = []
        firstEnumerator.enumerateItems(for: firstObserver, startingAt: NSFileProviderPage(Data(count: 1)))

        await fulfillment(of: [firstPass], timeout: 1)

        // Second pass under unchanged conditions: reconciliation must have converged.
        let secondPass = XCTestExpectation(description: "Second pass completion handler called")

        let secondObserver = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { newlyMaterialisedIds, unmaterialisedIds in
            XCTAssertTrue(
                newlyMaterialisedIds.isEmpty,
                "Nothing was enumerated, so nothing should be newly materialised."
            )
            XCTAssertTrue(
                unmaterialisedIds.isEmpty,
                "A repeated pass without changes must not report any evictions again."
            )

            let persistedVisitedDir = dbManager.itemMetadata(ocId: "visitedDir")
            XCTAssertTrue(
                persistedVisitedDir?.visitedDirectory ?? false,
                "The visited directory should still keep its visitedDirectory subscription."
            )

            let persistedStaleDir = dbManager.itemMetadata(ocId: "staleDir")
            XCTAssertTrue(
                persistedStaleDir?.visitedDirectory ?? false,
                "The stale directory should still keep its visitedDirectory subscription."
            )

            secondPass.fulfill()
        }

        let secondEnumerator = MockEnumerator(
            account: Self.account, dbManager: dbManager, remoteInterface: remoteInterface
        )
        secondEnumerator.enumeratorItems = []
        secondEnumerator.enumerateItems(for: secondObserver, startingAt: NSFileProviderPage(Data(count: 1)))

        await fulfillment(of: [secondPass], timeout: 1)
    }
}
