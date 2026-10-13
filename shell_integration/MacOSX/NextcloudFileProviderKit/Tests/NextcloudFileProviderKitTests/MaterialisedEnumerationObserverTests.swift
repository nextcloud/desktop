//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKit
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

    private func makeDatabaseManager() -> FilesDatabaseManager {
        let dbManager = FilesDatabaseManager(account: Self.account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())
        setUpDatabase(dbManager)
        return dbManager
    }

    private func downloadedFile(ocId: String, fileName: String? = nil) -> SendableItemMetadata {
        var metadata = SendableItemMetadata(ocId: ocId, fileName: fileName ?? "\(ocId).txt", account: Self.account)
        metadata.serverUrl = Self.account.davFilesUrl
        metadata.downloaded = true
        metadata.uploaded = true
        return metadata
    }

    private func materializedItem(_ metadata: SendableItemMetadata) -> MockFileProviderItem {
        MockFileProviderItem(identifier: .init(metadata.ocId), filename: metadata.fileName, isUploaded: metadata.uploaded)
    }

    /// Records `first-page.txt`, `second-page.txt` and `evicted.txt` as downloaded, then simulates
    /// a listing that returns the first two files on separate pages.
    /// The observer must wait for both pages, clear only `evicted.txt`'s `downloaded` flag, and
    /// call its completion handler once.
    func testMaterialisedObserverWaitsForTerminalPageBeforeReconciling() throws {
        let dbManager = makeDatabaseManager()
        let firstFile = downloadedFile(ocId: "first-page")
        let secondFile = downloadedFile(ocId: "second-page")
        let evictedFile = downloadedFile(ocId: "evicted")
        [firstFile, secondFile, evictedFile].forEach { dbManager.addItemMetadata($0) }
        var completionCount = 0
        var evictedIds = Set<NSFileProviderItemIdentifier>()
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { _, evicted in
            completionCount += 1
            evictedIds = evicted
        }

        observer.didEnumerate([materializedItem(firstFile)])
        observer.finishEnumerating(upTo: NSFileProviderPage(Data("second-page".utf8)))

        XCTAssertEqual(completionCount, 0)
        XCTAssertTrue(dbManager.itemMetadata(ocId: secondFile.ocId)?.downloaded == true)
        XCTAssertTrue(dbManager.itemMetadata(ocId: evictedFile.ocId)?.downloaded == true)

        observer.didEnumerate([materializedItem(secondFile)])
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

    /// Fails the listing before any results or after reporting downloaded `first.txt` and
    /// dataless `dataless.txt`; downloaded `unreported.txt` is absent in both cases.
    /// All three records must retain their flags, with no reported changes and one completion.
    func testMaterialisedObserverPreservesDownloadedStateAfterEnumerationError() throws {
        for reportsPartialListing in [false, true] {
            let dbManager = makeDatabaseManager()
            let firstFile = downloadedFile(ocId: "first")
            let unreportedFile = downloadedFile(ocId: "unreported")
            var datalessFile = downloadedFile(ocId: "dataless")
            datalessFile.downloaded = false
            dbManager.addItemMetadata(firstFile)
            dbManager.addItemMetadata(unreportedFile)
            dbManager.addItemMetadata(datalessFile)
            var completionCount = 0
            var materializedIds = Set<NSFileProviderItemIdentifier>()
            var evictedIds = Set<NSFileProviderItemIdentifier>()
            let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { materialized, evicted in
                completionCount += 1
                materializedIds = materialized
                evictedIds = evicted
            }

            if reportsPartialListing {
                observer.didEnumerate([materializedItem(firstFile), materializedItem(datalessFile)])
            }
            observer.finishEnumeratingWithError(NSFileProviderError(.serverUnreachable))

            XCTAssertEqual(completionCount, 1)
            let storedFirstFile = try XCTUnwrap(dbManager.itemMetadata(ocId: firstFile.ocId))
            let storedUnreportedFile = try XCTUnwrap(dbManager.itemMetadata(ocId: unreportedFile.ocId))
            XCTAssertTrue(storedFirstFile.downloaded)
            XCTAssertTrue(storedUnreportedFile.downloaded)
            XCTAssertTrue(dbManager.itemMetadata(ocId: datalessFile.ocId)?.downloaded == false)
            XCTAssertTrue(materializedIds.isEmpty)
            XCTAssertTrue(evictedIds.isEmpty)
            XCTAssertFalse(storedFirstFile.deleted)
            XCTAssertFalse(storedUnreportedFile.deleted)
            XCTAssertTrue(storedFirstFile.uploaded)
            XCTAssertTrue(storedUnreportedFile.uploaded)
        }
    }

    /// Lists `first-page.txt` and `second-page.txt` around an empty intermediate page.
    /// Every returned page token must be requested, with database updates deferred until the
    /// final page; only the absent downloaded file `evicted.txt` may become dataless.
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

    /// Delays both page callbacks after the caller releases its enumerator reference.
    /// The observer must keep the enumerator alive to request the next page, then release it
    /// after the final page while preserving the listed file's downloaded state.
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

    /// Reports dataless `dataless.txt` on the first page, then fails the next page.
    /// Neither that file nor absent downloaded `downloaded.txt` may change state, and the
    /// enumerator must be invalidated with one completion and no reported changes.
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

    /// Sends more items and completion callbacks after a listing has succeeded or failed.
    /// These late callbacks must not change either file's state or complete the listing again.
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

    /// Starts with `existing.txt` and `evicted.txt` marked downloaded, and reports only
    /// `existing.txt` in the simulated listing.
    /// The test adds a downloaded record for `newly-uploaded.txt` before finishing that listing.
    /// Only `evicted.txt` should lose its `downloaded` flag; `newly-uploaded.txt` was added too
    /// late for the older listing to describe its local contents.
    func testMaterialisedObserverPreservesUploadCompletedDuringEnumeration() throws {
        let dbManager = makeDatabaseManager()
        let existingFile = downloadedFile(ocId: "existing")
        let evictedFile = downloadedFile(ocId: "evicted")
        dbManager.addItemMetadata(existingFile)
        dbManager.addItemMetadata(evictedFile)
        var completionCount = 0
        var evictedIds = Set<NSFileProviderItemIdentifier>()
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { _, evicted in
            completionCount += 1
            evictedIds = evicted
        }
        observer.didEnumerate([materializedItem(existingFile)])

        // Upload completion persists the server identifier after the system has taken its listing.
        let uploadedFile = downloadedFile(ocId: "newly-uploaded")
        dbManager.addItemMetadata(uploadedFile)
        observer.finishEnumerating(upTo: nil)

        XCTAssertEqual(completionCount, 1)
        let storedUpload = try XCTUnwrap(dbManager.itemMetadata(ocId: uploadedFile.ocId))
        XCTAssertTrue(evictedIds.contains(NSFileProviderItemIdentifier(evictedFile.ocId)))
        XCTExpectFailure("New upload rows are currently compared against an older materialized listing.") {
            XCTAssertTrue(storedUpload.downloaded)
            XCTAssertFalse(evictedIds.contains(NSFileProviderItemIdentifier(uploadedFile.ocId)))
        }
        XCTAssertFalse(storedUpload.deleted)
        XCTAssertTrue(storedUpload.uploaded)
        XCTAssertEqual(storedUpload.fileName, uploadedFile.fileName)
        XCTAssertTrue(dbManager.itemMetadata(ocId: existingFile.ocId)?.downloaded == true)
        XCTAssertTrue(dbManager.itemMetadata(ocId: evictedFile.ocId)?.downloaded == false)
    }

    /// Stores `copied.txt` under identifier `server-ocid`, then reports the same filename to
    /// the observer with unknown identifier `temporary-import-id`.
    /// The `server-ocid` record must retain its identifier, filename, ETag and uploaded status,
    /// remain undeleted, and no record may be created for `temporary-import-id`.
    func testMaterialisedObserverDoesNotReplaceServerMetadataWithTemporaryIdentifier() throws {
        let dbManager = makeDatabaseManager()
        var serverFile = downloadedFile(ocId: "server-ocid", fileName: "copied.txt")
        serverFile.etag = "uploaded-etag"
        dbManager.addItemMetadata(serverFile)
        let temporaryIdentifier = NSFileProviderItemIdentifier("temporary-import-id")
        var completionCount = 0
        let observer = MaterializedEnumerationObserver(account: Self.account, dbManager: dbManager, log: FileProviderLogMock()) { _, _ in
            completionCount += 1
        }
        observer.didEnumerate([MockFileProviderItem(identifier: temporaryIdentifier, filename: serverFile.fileName, isUploaded: false)])
        observer.finishEnumerating(upTo: nil)

        XCTAssertEqual(completionCount, 1)
        XCTAssertNil(dbManager.itemMetadata(temporaryIdentifier))
        let storedFile = try XCTUnwrap(dbManager.itemMetadata(ocId: serverFile.ocId))
        XCTAssertEqual(storedFile.ocId, serverFile.ocId)
        XCTAssertEqual(storedFile.fileName, serverFile.fileName)
        XCTAssertEqual(storedFile.etag, serverFile.etag)
        XCTAssertFalse(storedFile.deleted)
        XCTAssertTrue(storedFile.uploaded)
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
