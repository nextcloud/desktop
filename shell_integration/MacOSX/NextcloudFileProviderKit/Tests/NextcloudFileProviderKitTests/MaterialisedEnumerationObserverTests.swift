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
