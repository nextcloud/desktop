//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

//
//  Reproducing tests for "server-side changes intermittently do not surface in the
//  macOS File Provider domain". See the investigation plan
//  (i-frequently-receive-reports-clever-karp).
//
//  Background
//  ----------
//  When the main app learns of a remote change (notify_push fileId / files signal, or
//  root-ETag polling) it ultimately calls `notifyChange()`, which signals ONLY the
//  `.workingSet` enumerator. So the working-set change path is the only extension code a
//  push/poll signal drives. That path is:
//
//      enumerateChanges(.workingSet)
//        -> scanMaterialisedItemsForRemoteChanges()  // PROPFINDs the pushed items, or every
//                                                    // *materialized* item, and writes fresh
//                                                    // metadata + syncTime to the DB
//        -> pendingWorkingSetChanges(since: anchor)  // re-derives the report from the DB by
//                                                    // `syncTime > anchorDate` (+ a child scan)
//        -> observer.didUpdate / didDeleteItems
//
//  These tests mutate the mock "server" and then run the *real* working-set path, asserting
//  the change reaches `MockChangeObserver`. The header comment of each test records which
//  suspect (S1/S2/S4) it probes.
//
//  Three of them reproduced real silent-drops in the enumerator/DB change-derivation logic and
//  now pass after the fixes (see inline "Regression guard (Sx)" notes):
//    - S1/S2: `scanMaterialisedItemsForRemoteChanges` now returns the changes it discovers (reported
//      directly) and recurses into changed subdirectories, instead of relying on the lossy
//      `syncTime`-based reconstruction in `pendingWorkingSetChanges`.
//    - S4: `isInSameDatabaseStoreableRemoteState` now also compares `size`.
//
//  None of these tests require any test-only mock changes.
//

@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
@testable import TestInterface
import XCTest

/// Thread-safe collector for the remote paths a working-set scan issues PROPFINDs for. The mock's
/// `enumerateCallHandler` may run off the test's thread, so guard the storage with a lock.
private final class EnumeratePathRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private var storage: [String] = []

    func add(_ path: String) {
        lock.lock()
        defer { lock.unlock() }
        storage.append(path)
    }

    var paths: [String] {
        lock.lock()
        defer { lock.unlock() }
        return storage
    }
}

final class RemoteChangePropagationTests: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    static let dbManager = try! FilesDatabaseManager(
        account: account,
        databaseDirectory: makeDatabaseDirectory(),
        fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
        log: FileProviderLogMock()
    )

    // Seeded rows are stamped well before the anchor so they do not spuriously match the
    // `syncTime > anchorDate` window; the working-set scan stamps fresh rows at "now".
    let oldSyncTime = Date(timeIntervalSinceNow: -600) // 10 minutes ago
    let anchorDate = Date(timeIntervalSinceNow: -300) // 5 minutes ago

    var rootItem: MockRemoteItem!

    override func setUp() {
        super.setUp()
        try! Self.dbManager.removeAllRowsForTesting()
        rootItem = MockRemoteItem.rootItem(account: Self.account)
    }

    // MARK: - Helpers

    /// Seed a DB row from `item`'s *current* state, with explicit materialization flags and a
    /// pre-anchor `syncTime`. Call this BEFORE mutating the mock item's `versionIdentifier`, so the
    /// DB holds the "old" state and the mock returns the "new" state on the next PROPFIND.
    @discardableResult
    private func seed(
        _ item: MockRemoteItem,
        downloaded: Bool = false,
        visitedDirectory: Bool = false,
        keepDownloaded: Bool = false,
        syncTime: Date? = nil
    ) -> SendableItemMetadata {
        var metadata = item.toItemMetadata(account: Self.account)
        metadata.downloaded = downloaded
        metadata.visitedDirectory = visitedDirectory
        metadata.keepDownloaded = keepDownloaded
        metadata.syncTime = syncTime ?? oldSyncTime
        Self.dbManager.addItemMetadata(metadata)
        return metadata
    }

    private func makeFolder(name: String, parent: MockRemoteItem, etag: String) -> MockRemoteItem {
        let folder = MockRemoteItem(
            identifier: name,
            versionIdentifier: etag,
            name: name,
            remotePath: parent.remotePath + "/" + name,
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        folder.parent = parent
        parent.children.append(folder)
        return folder
    }

    private func makeFile(
        name: String, parent: MockRemoteItem, etag: String, data: Data = Data([1, 2, 3])
    ) -> MockRemoteItem {
        let file = MockRemoteItem(
            identifier: name,
            versionIdentifier: etag,
            name: name,
            remotePath: parent.remotePath + "/" + name,
            data: data,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        file.parent = parent
        parent.children.append(file)
        return file
    }

    private func runWorkingSetChanges(
        _ remoteInterface: MockRemoteInterface,
        remoteChangeTargets: RemoteChangeTargets = RemoteChangeTargets()
    ) async throws -> MockChangeObserver {
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            remoteChangeTargets: remoteChangeTargets,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: anchorDate, storeIdentity: Self.dbManager.storeIdentity))
        return observer
    }

    private func reportedIds(_ observer: MockChangeObserver) -> Set<String> {
        Set(observer.changedItems.map(\.itemIdentifier.rawValue))
    }

    // MARK: - Tests

    /// Baseline (sanity): a downloaded (materialized) file that changed on the server is reported.
    /// Materialized files are PROPFIND'd at `.target` depth and re-stamped every scan, so this is
    /// expected to PASS — it confirms the harness and the happy path.
    func testMaterializedFileChangeIsReported() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let file = makeFile(name: "itemA", parent: folder, etag: "itemA-v1")

        seed(folder, visitedDirectory: true)
        seed(file, downloaded: true) // materialized

        // Server-side change to the materialized file.
        file.versionIdentifier = "itemA-v2"
        file.modificationDate = Date()

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertNil(observer.error)
        XCTAssertTrue(
            reportedIds(observer).contains(file.identifier),
            "A materialized file changed on the server must be reported as updated."
        )
    }

    /// Regression for ticket 96101301: an app "safe save" (create -> delete -> recreate) rotates the
    /// ocId, so one logical path ends up with a stale ghost row (gone from the server) beside the live,
    /// recreated file. During the working-set scan the parent depth-1 read reconciles the stale ocId as
    /// gone and the scan persists that tombstone via `addItemMetadata`. Before the fix, the tombstone's
    /// `evictLogicalDuplicates` soft-deleted the LIVE sibling, and both ocIds were reported deleted — so
    /// the freshly saved file vanished from Finder even though it was live on the server. The live row
    /// must survive and be reported updated (not deleted); the genuinely-gone ghost must still be
    /// reported deleted.
    func testAtomicSaveRecreateDoesNotDeleteLiveItemFromWorkingSet() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        // The live, recreated file that currently exists on the server at this path.
        let liveFile = makeFile(name: "TEST 1.pdf", parent: folder, etag: "pdf-v1")

        seed(folder, visitedDirectory: true)
        seed(liveFile, downloaded: true) // materialized live file

        // Seed a stale ghost row at the SAME logical address (same serverUrl + fileName) but a
        // different ocId, NOT present on the mock server. `uploaded == true` so the depth-1 read's
        // delete reconciliation considers it. This stands in for the first "safe save" write whose
        // ocId the server rotated away.
        let storedLive = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: liveFile.identifier))
        var ghost = SendableItemMetadata.rawRow(ocId: "staleGhost")
        ghost.account = Self.account.ncKitAccount
        ghost.serverUrl = storedLive.serverUrl
        ghost.fileName = storedLive.fileName
        ghost.uploaded = true
        ghost.syncTime = oldSyncTime
        try Self.dbManager.insertForTesting(ghost)

        // The live file also changed on the server (the recreate), so it is a genuine update.
        liveFile.versionIdentifier = "pdf-v2"
        liveFile.modificationDate = Date()

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertNil(observer.error)

        // The live row must NOT be soft-deleted by the ghost's eviction (primary regression guard).
        let liveAfter = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: liveFile.identifier))
        XCTAssertFalse(
            liveAfter.deleted,
            "The live recreated file must not be soft-deleted when the stale ghost's tombstone is persisted."
        )

        let deletedIds = Set(observer.deletedItemIdentifiers.map(\.rawValue))
        XCTAssertFalse(
            deletedIds.contains(liveFile.identifier),
            "The live recreated file must not be reported deleted to the framework."
        )
        XCTAssertTrue(
            reportedIds(observer).contains(liveFile.identifier),
            "The live recreated file must be reported as an update."
        )
        XCTAssertTrue(
            deletedIds.contains("staleGhost"),
            "The genuinely-gone stale ocId must still be reported deleted."
        )
    }

    /// A delivered deletion must remain durable until the framework acknowledges the finished batch.
    /// The first observer is paused immediately before its finish callback returns; a second
    /// enumerator using the original anchor must then receive the same downloaded and placeholder
    /// deletions instead of seeing an already-consumed queue.
    func testWorkingSetDeletionIsReplayedUntilObserverAcknowledges() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let downloaded = makeFile(name: "downloaded", parent: folder, etag: "downloaded-v1")
        let placeholder = makeFile(name: "placeholder", parent: folder, etag: "placeholder-v1")

        seed(folder, visitedDirectory: true)
        seed(downloaded, downloaded: true)
        seed(placeholder)

        folder.children = []

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let inputAnchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: Self.dbManager.storeIdentity)
        let firstEnumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let firstObserver = MockChangeObserver(enumerator: firstEnumerator)
        let finishEntered = expectation(description: "First observer reaches the acknowledgement boundary")
        let releaseFinish = DispatchSemaphore(value: 0)
        firstObserver.beforeFinishEnumeratingChanges = {
            finishEntered.fulfill()
            _ = releaseFinish.wait(timeout: .distantFuture)
        }
        defer { releaseFinish.signal() }

        firstEnumerator.enumerateChanges(for: firstObserver, from: inputAnchor)

        await fulfillment(of: [finishEntered], timeout: 5)
        XCTAssertEqual(
            Set(firstObserver.deletedItemIdentifiers.map(\.rawValue)),
            Set([downloaded.identifier, placeholder.identifier])
        )

        let secondEnumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let secondObserver = MockChangeObserver(enumerator: secondEnumerator)
        let expectedDeletedIds = Set([downloaded.identifier, placeholder.identifier])
        let replayExpectation = expectation(description: "Second observer receives the replayed deletions")
        let secondFinishExpectation = expectation(description: "Second observer finishes the replayed batch")
        secondObserver.didDeleteItemsHandler = { identifiers in
            if Set(identifiers.map(\.rawValue)) == expectedDeletedIds {
                replayExpectation.fulfill()
            }
        }
        secondObserver.beforeFinishEnumeratingChanges = {
            secondFinishExpectation.fulfill()
        }
        // The first observer deliberately blocks the main actor until its acknowledgement is released.
        // Start the replacement enumerator from a background queue so it can observe the durable pending
        // batch while that callback is still blocked, matching File Provider's independent callbacks.
        DispatchQueue.global().async {
            secondEnumerator.enumerateChanges(for: secondObserver, from: inputAnchor)
        }

        await fulfillment(of: [replayExpectation], timeout: 5)

        XCTAssertNil(secondObserver.error)
        XCTAssertEqual(
            Set(secondObserver.deletedItemIdentifiers.map(\.rawValue)),
            Set([downloaded.identifier, placeholder.identifier]),
            "An interrupted delivery must be replayed from the original anchor."
        )

        let cleanupExpectation = XCTNSPredicateExpectation(
            predicate: NSPredicate { _, _ in
                Self.dbManager.itemMetadata(ocId: downloaded.identifier) == nil
                    && Self.dbManager.itemMetadata(ocId: placeholder.identifier) == nil
            },
            object: NSObject()
        )
        releaseFinish.signal()

        await fulfillment(of: [cleanupExpectation, secondFinishExpectation], timeout: 5)
        XCTAssertNil(firstObserver.error)
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: downloaded.identifier))
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: placeholder.identifier))
    }

    /// Scenario A (S1/S2 control): a NON-materialized file changed inside a visited (materialized)
    /// folder, AND the folder's ETag is bumped too (a well-behaved server propagates child changes
    /// up to the parent's ETag). Expected to PASS — the parent's bumped `syncTime` lets the child
    /// scan in `pendingWorkingSetChanges` pick up the changed child.
    func testChangedChildSurfacesWhenParentEtagAlsoBumped() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let file = makeFile(name: "itemA", parent: folder, etag: "itemA-v1")

        seed(folder, visitedDirectory: true)
        seed(file, downloaded: false) // NOT materialized

        // Child changed; parent ETag bumped to reflect it (well-behaved server).
        file.versionIdentifier = "itemA-v2"
        file.modificationDate = Date()
        folder.versionIdentifier = "folder-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertNil(observer.error)
        XCTAssertTrue(
            reportedIds(observer).contains(file.identifier),
            "A changed child in a visited folder should be reported when the parent ETag is bumped."
        )
    }

    /// Scenario B (S1/S2): same as A but the parent folder's ETag is NOT bumped. This probes how
    /// strongly the working-set report depends on the parent directory's `syncTime` being advanced.
    /// The change is written to the DB by `scanMaterialisedItemsForRemoteChanges` but
    /// `pendingWorkingSetChanges` only scans children of directories that are themselves in the
    /// materialized-and-changed set — so a non-materialized child whose parent did not change may be
    /// dropped. Asserts the desired behaviour (child reported); a FAILURE documents the dependency.
    func testChangedChildSurfacesWhenParentEtagUnchanged() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let file = makeFile(name: "itemA", parent: folder, etag: "itemA-v1")

        seed(folder, visitedDirectory: true)
        seed(file, downloaded: false) // NOT materialized

        // Child changed; parent ETag deliberately left unchanged.
        file.versionIdentifier = "itemA-v2"
        file.modificationDate = Date()

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertNil(observer.error)
        // Regression guard (S1/S2): the changed child is now reported because
        // scanMaterialisedItemsForRemoteChanges returns it directly, rather than being gated on the
        // parent folder's syncTime via pendingWorkingSetChanges.
        XCTAssertTrue(
            reportedIds(observer).contains(file.identifier),
            "A changed non-materialized child should surface even if the parent folder ETag did not change."
        )
    }

    /// Scenario C (S1 headline — bounded recursion): a change to a grandchild file under a NON-visited
    /// subdirectory of a visited folder. `scanMaterialisedItemsForRemoteChanges` reads the visited
    /// folder only at depth 1, so it sees the changed subdirectory but not the grandchild. The fix
    /// enqueues the changed subdirectory and scans it — BUT only because the subtree contains a
    /// materialised item (a downloaded sibling `anchor`), which is what makes the working set care
    /// about it. So the grandchild's change is discovered, persisted and reported rather than dropped
    /// behind the depth-1 read. The companion test below proves the complementary bound: a changed
    /// subtree with NOTHING materialised inside is deliberately NOT crawled.
    func testDeepChangeUnderNonVisitedSubfolderWithMaterializedDescendantIsReported() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let childFolder = makeFolder(name: "childFolder", parent: folder, etag: "child-v1")
        let grandchild = makeFile(name: "itemX", parent: childFolder, etag: "itemX-v1")
        // A downloaded (materialized) sibling makes `childFolder`'s subtree part of the working set,
        // so the scan is allowed to descend into the changed `childFolder`.
        let anchor = makeFile(name: "anchor", parent: childFolder, etag: "anchor-v1")

        seed(folder, visitedDirectory: true) // materialized
        seed(childFolder, visitedDirectory: false) // NOT materialized, but known in DB
        seed(grandchild, downloaded: false) // NOT materialized, but known in DB
        seed(anchor, downloaded: true) // materialized descendant — justifies descent

        // Well-behaved server: every ancestor ETag bumped along with the grandchild's change.
        grandchild.versionIdentifier = "itemX-v2"
        grandchild.modificationDate = Date()
        childFolder.versionIdentifier = "child-v2"
        folder.versionIdentifier = "folder-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertNil(observer.error)
        // Regression guard (S1 headline): scanMaterialisedItemsForRemoteChanges enqueues the changed
        // subfolder (its subtree holds a materialised item) and scans it, so the grandchild surfaces.
        XCTAssertTrue(
            reportedIds(observer).contains(grandchild.identifier),
            "A change to a grandchild under a non-visited subfolder with a materialized descendant should surface."
        )
    }

    /// Scenario D (PROPFIND-storm guard): a sibling subtree that the server reports as changed (its
    /// ETag bubbled up) but which the user NEVER opened — nothing inside it is materialised, and its
    /// children are brand new on the server. `scanMaterialisedItemsForRemoteChanges` must report the
    /// sibling directory's own change but must NOT recurse into it; otherwise a single working-set
    /// signal on a freshly activated / sparse domain walks entire never-visited subtrees (observed:
    /// ~1700 PROPFINDs to depth 7 from one push). Verifies no enumerate call is ever issued into the
    /// sibling subtree and that its never-enumerated descendants are not reported.
    func testChangedUnmaterializedSiblingSubtreeIsNotCrawled() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")

        // A `Talk`-like sibling: known in the DB (it was listed when `folder`'s parent was enumerated)
        // but never visited, so nothing under it is materialized.
        let siblingFolder = makeFolder(name: "SiblingFolder", parent: folder, etag: "sibling-v1")
        // Children that exist on the server but have never been enumerated -> no DB rows -> classified
        // NEW; these are exactly what the old code crawled into.
        let newSub = makeFolder(name: "NewSub", parent: siblingFolder, etag: "newsub-v1")
        let newLeaf = makeFile(name: "newLeaf", parent: newSub, etag: "newleaf-v1")

        seed(folder, visitedDirectory: true) // materialized -> seeds the scan
        seed(siblingFolder, visitedDirectory: false) // known but NOT materialized
        // newSub / newLeaf intentionally NOT seeded -> NEW on the next read.

        // Server bumps the sibling's ETag (a change happened somewhere inside it).
        siblingFolder.versionIdentifier = "sibling-v2"
        folder.versionIdentifier = "folder-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Record every path the scan issues a PROPFIND for.
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }

        let observer = try await runWorkingSetChanges(remoteInterface)
        let enumeratedPaths = recorder.paths

        XCTAssertNil(observer.error)

        // Sanity: the materialized folder itself is scanned at depth 1.
        XCTAssertTrue(
            enumeratedPaths.contains { $0.hasSuffix("/folder") },
            "The materialized folder should be read."
        )
        // The bound: the unmaterialized sibling subtree is never crawled.
        XCTAssertFalse(
            enumeratedPaths.contains { $0.contains("/SiblingFolder") },
            "A changed-but-unmaterialized sibling subtree must not be PROPFIND'd (storm guard)."
        )
        // Its never-enumerated descendants are therefore not reported.
        XCTAssertFalse(
            reportedIds(observer).contains(newSub.identifier),
            "A never-visited NEW subdirectory must not surface in the working set."
        )
        XCTAssertFalse(
            reportedIds(observer).contains(newLeaf.identifier),
            "A never-visited NEW leaf must not surface in the working set."
        )
        // The sibling directory's OWN change is still reported — we report the change to the known
        // item, we just do not crawl its contents.
        XCTAssertTrue(
            reportedIds(observer).contains(siblingFolder.identifier),
            "The changed (known) sibling directory's own update should still be reported."
        )
    }

    /// Run one working-set scan after a folder with a file in it was created on the server inside a visited folder, pinned or not.
    private func scanAfterNewFolder(underPinnedParent parentIsPinned: Bool) async throws -> (observer: MockChangeObserver, enumeratedPaths: [String], newFolder: MockRemoteItem, newLeaf: MockRemoteItem) {
        let parentFolder = makeFolder(name: "parentFolder", parent: rootItem, etag: "parent-v1")

        // Brand new on the server, so nothing inside it is materialised to justify descending.
        let newFolder = makeFolder(name: "NewFolder", parent: parentFolder, etag: "newfolder-v1")
        let newLeaf = makeFile(name: "newLeaf", parent: newFolder, etag: "newleaf-v1")

        seed(parentFolder, visitedDirectory: true, keepDownloaded: parentIsPinned)

        parentFolder.versionIdentifier = "parent-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }

        let observer = try await runWorkingSetChanges(remoteInterface)
        return (observer, recorder.paths, newFolder, newLeaf)
    }

    /// A folder created on the server inside a pinned folder should be read in the same working-set scan.
    func testNewFolderUnderAPinnedFolderIsCrawled() async throws {
        let (observer, enumeratedPaths, _, newLeaf) = try await scanAfterNewFolder(underPinnedParent: true)

        XCTAssertNil(observer.error)
        XCTAssertTrue(
            enumeratedPaths.contains { $0.hasSuffix("/NewFolder") },
            "A new directory inside a pinned folder should be read in the same working-set scan."
        )
        XCTAssertTrue(
            reportedIds(observer).contains(newLeaf.identifier),
            "A file created on the server inside a new folder under a pinned folder should surface."
        )
    }

    /// A folder created on the server inside an unpinned folder is reported but not read, because nothing inside it is part of the working set.
    func testNewFolderUnderAnUnpinnedFolderIsNotCrawled() async throws {
        let (observer, enumeratedPaths, newFolder, newLeaf) = try await scanAfterNewFolder(underPinnedParent: false)

        XCTAssertNil(observer.error)
        XCTAssertFalse(
            enumeratedPaths.contains { $0.hasSuffix("/NewFolder") },
            "A new directory inside an unpinned folder should not be read by the working-set scan."
        )
        XCTAssertFalse(
            reportedIds(observer).contains(newLeaf.identifier),
            "A file inside a new folder under an unpinned folder should not be reported by the working-set scan."
        )
        XCTAssertTrue(
            reportedIds(observer).contains(newFolder.identifier),
            "The new folder itself should still be reported."
        )
    }

    /// A rename landing while the scan's read of the old path is in flight must not turn that read's 404 into a deletion.
    func testA404ForAPathTheItemHasMovedAwayFromIsNotADeletion() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let seeded = seed(folder, visitedDirectory: true)

        // The server has already accepted the rename, so the path the scan holds 404s.
        rootItem.children.removeAll { $0 === folder }

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // The rename reaches the database while the read of the old path is in flight.
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            guard remotePath.hasSuffix("/folder") else { return }

            var renamed = seeded
            renamed.fileName = "renamedFolder"
            renamed.fileNameView = "renamedFolder"
            Self.dbManager.addItemMetadata(renamed)
        }

        let observer = try await runWorkingSetChanges(remoteInterface)
        let deletedIds = Set(observer.deletedItemIdentifiers.map(\.rawValue))

        XCTAssertNil(observer.error)
        XCTAssertFalse(
            deletedIds.contains(folder.identifier),
            "A 404 for a path the item has since been renamed away from must not be reported as a deletion."
        )
        XCTAssertEqual(
            Self.dbManager.itemMetadata(ocId: folder.identifier)?.deleted, false,
            "The stale 404 must not mark the moved item's row deleted."
        )
        XCTAssertEqual(
            Self.dbManager.itemMetadata(ocId: folder.identifier)?.fileName, "renamedFolder",
            "The stale 404 must not rewrite the row back to its pre-rename name."
        )
    }

    /// A rename whose MOVE response is handled after the 404 for the old path, while the scan reads a later wave, must not turn that 404 into a deletion.
    func testA404WhoseRenameLandsDuringALaterReadIsNotADeletion() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let other = makeFolder(name: "other", parent: rootItem, etag: "other-v1")
        let laterFolder = makeFolder(name: "later", parent: other, etag: "later-v1")
        let seeded = seed(folder, visitedDirectory: true)
        seed(laterFolder, visitedDirectory: true)
        rootItem.children.removeAll { $0 === folder }

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            guard remotePath.hasSuffix("/other/later") else { return }

            var renamed = seeded
            renamed.fileName = "renamedFolder"
            renamed.fileNameView = "renamedFolder"
            Self.dbManager.addItemMetadata(renamed)
        }

        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertFalse(observer.deletedItemIdentifiers.map(\.rawValue).contains(folder.identifier))
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: folder.identifier)?.fileName, "renamedFolder")
    }

    /// A child that a folder's depth-1 read found deleted stays deleted when a folder above it is renamed in a later wave of the scan.
    func testADepth1DeletionIsReportedWhenAParentIsRenamedDuringTheScan() async throws {
        let folderA = makeFolder(name: "A", parent: rootItem, etag: "a-v1")
        let folderP = makeFolder(name: "P", parent: folderA, etag: "p-v1")
        let child = makeFile(name: "c", parent: folderP, etag: "c-v1")
        let other = makeFolder(name: "other", parent: rootItem, etag: "other-v1")
        let sub = makeFolder(name: "sub", parent: other, etag: "sub-v1")
        let laterFolder = makeFolder(name: "later", parent: sub, etag: "later-v1")
        seed(folderA, visitedDirectory: true)
        seed(folderP, visitedDirectory: true)
        seed(child)
        seed(laterFolder, visitedDirectory: true)

        // A collaborator deletes c, which changes the ETags above it.
        folderP.children.removeAll { $0 === child }
        folderP.versionIdentifier = "p-v2"
        folderA.versionIdentifier = "a-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let rootUrl = Self.account.davFilesUrl

        // The user's rename of A lands while the scan reads a deeper wave, after /A/P was merged.
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            guard remotePath.hasSuffix("/other/sub/later") else { return }
            _ = Self.dbManager.renameDirectoryAndPropagateToChildren(ocId: folderA.identifier, newServerUrl: rootUrl, newFileName: "A2")
        }

        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertTrue(observer.deletedItemIdentifiers.map(\.rawValue).contains(child.identifier))
    }

    /// An upload that starts during the scan keeps a file whose read returned 404 from being deleted.
    func testAnUploadStartedDuringTheScanBlocksTheDeletion() async throws {
        let file = makeFile(name: "f", parent: rootItem, etag: "f-v1")
        let laterFolder = makeFolder(name: "zzLaterFolder", parent: rootItem, etag: "later-v1")
        let seededFile = seed(file, downloaded: true)
        seed(laterFolder, visitedDirectory: true)

        rootItem.children.removeAll { $0 === file }

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // The user saves f in an editor while the scan reads a later item.
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            guard remotePath.hasSuffix("/zzLaterFolder") else { return }
            _ = Self.dbManager.setStatusForItemMetadata(seededFile, status: .uploading)
        }

        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertFalse(observer.deletedItemIdentifiers.map(\.rawValue).contains(file.identifier))
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: file.identifier)?.deleted, false)
    }

    /// A downloaded file moved into a folder the scan does not read must not be reported deleted by the 404 at its old path.
    func testAFileMovedIntoAnUnvisitedFolderIsNotADeletion() async throws {
        seed(rootItem, visitedDirectory: true)
        let source = makeFolder(name: "source", parent: rootItem, etag: "source-v1")
        let target = makeFolder(name: "target", parent: rootItem, etag: "target-v1")
        let file = makeFile(name: "f", parent: source, etag: "f-v1")
        seed(source, visitedDirectory: true)
        seed(target)
        seed(file, downloaded: true)

        // The server has already accepted the move.
        source.children.removeAll { $0 === file }
        target.children.append(file)
        file.parent = target
        file.remotePath = target.remotePath + "/f"
        source.versionIdentifier = "source-v2"
        target.versionIdentifier = "target-v2"
        rootItem.versionIdentifier = "root-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let targetUrl = Self.account.davFilesUrl + "/target"

        // The MOVE response is handled while the scan reads the old folder.
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            guard remotePath.hasSuffix("/source") else { return }
            Self.dbManager.renameItemMetadata(ocId: file.identifier, newServerUrl: targetUrl, newFileName: "f")
        }

        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertFalse(observer.deletedItemIdentifiers.map(\.rawValue).contains(file.identifier))
    }

    /// S4: the change-detection predicate `isInSameDatabaseStoreableRemoteState` keys on ETag (+ a
    /// fixed field set). If a file's content changes but its ETag is unchanged, the predicate treats
    /// it as unchanged and the update is skipped. Real servers bump the ETag on content change, so
    /// this documents the predicate's reliance on a correct ETag rather than a likely field bug.
    func testSameEtagContentChangeIsReported() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let file = makeFile(name: "itemA", parent: folder, etag: "itemA-v1", data: Data([1, 2, 3]))

        seed(folder, visitedDirectory: true)
        seed(file, downloaded: false)

        // Content (and parent ETag) change, but the file's own ETag and date are unchanged.
        file.data = Data([9, 9, 9, 9, 9, 9])
        folder.versionIdentifier = "folder-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertNil(observer.error)
        // Regression guard (S4): isInSameDatabaseStoreableRemoteState now also compares `size`, so a
        // content change is detected even if the server returned a stale/unchanged ETag and date.
        XCTAssertTrue(
            reportedIds(observer).contains(file.identifier),
            "A content change should surface; if it does not, the report depends entirely on a correct server ETag."
        )
    }

    /// Headline regression for this fix (nextcloud/desktop#10442): a single unreadable folder must
    /// NOT abort the whole working-set remote-change scan. Two visited folders each hold a changed
    /// downloaded file; one folder's PROPFIND is forced to fail with a non-404 server
    /// error (standing in for a large container timing out — the position the account root occupies in
    /// production, since the scan queue is sorted parent-first). The change in the OTHER folder must
    /// still be discovered and reported. Before the fix `scanMaterialisedItemsForRemoteChanges` `break`ed
    /// on the first failure and silently dropped every later folder's changes (the "files uploaded on
    /// the web never appear" bug). The incomplete scan must also NOT advance the working-set sync point,
    /// so the next signal re-derives and can pick up the folder it could not read this pass.
    func testWorkingSetScanContinuesPastAFailedFolderRead() async throws {
        // Both folders are read in the same wave; the name "a" also matches no path below it in the mock's suffix-based error injection.
        let failingFolder = makeFolder(name: "a", parent: rootItem, etag: "a-v1")
        let healthyFolder = makeFolder(name: "zzzzzzzzzz", parent: rootItem, etag: "z-v1")
        let fileInFailing = makeFile(name: "itemA", parent: failingFolder, etag: "itemA-v1")
        let fileInHealthy = makeFile(name: "itemZ", parent: healthyFolder, etag: "itemZ-v1")

        seed(failingFolder, visitedDirectory: true)
        seed(healthyFolder, visitedDirectory: true)
        seed(fileInFailing, downloaded: true)
        seed(fileInHealthy, downloaded: true)

        // Both downloaded files changed on the server.
        fileInFailing.versionIdentifier = "itemA-v2"; fileInFailing.modificationDate = Date()
        fileInHealthy.versionIdentifier = "itemZ-v2"; fileInHealthy.modificationDate = Date()

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        // Force one folder's depth-1 read to fail with a non-404 error (not a deletion).
        remoteInterface.enumerateErrorBySuffix = [
            "/a": NKError(statusCode: 500, fallbackDescription: "Internal Server Error")
        ]
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }

        let inputAnchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: Self.dbManager.storeIdentity)
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges(from: inputAnchor)

        // The scan skips the failing folder and finishes normally (no error surfaced to the framework)…
        XCTAssertNil(observer.error)
        // …and the change in the other folder still surfaces — the core regression.
        XCTAssertTrue(
            reportedIds(observer).contains(fileInHealthy.identifier),
            "A read failure on one folder must not prevent a later folder's change from being reported."
        )
        let enumeratedPaths = recorder.paths
        XCTAssertTrue(
            enumeratedPaths.contains { $0.hasSuffix("/a/itemA") },
            "The scan must go on to the next depth after a failed read, so the failed folder's downloaded file is read on its own. Got: \(enumeratedPaths)"
        )
        XCTAssertFalse(
            enumeratedPaths.contains { $0.hasSuffix("/zzzzzzzzzz/itemZ") },
            "The healthy folder's result must still be merged, and its read covers its file. Got: \(enumeratedPaths)"
        )
        // The incomplete scan must keep the incoming sync anchor rather than advancing to a fresh "now",
        // so the framework does not treat the domain as fully synced past the folder it could not read.
        let finalAnchor = try XCTUnwrap(observer.finishes.last?.anchor)
        XCTAssertEqual(
            finalAnchor.rawValue, inputAnchor.rawValue,
            "An incomplete working-set scan must not advance the working-set sync anchor."
        )
    }

    /// Headline regression for nextcloud/desktop#9688 and #10681: a folder the user creates on the Mac,
    /// then an item created inside it on the server, which stays invisible unless creation marks the
    /// folder visited.
    func testItemCreatedOnServerInLocallyCreatedFolderIsReported() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)

        // Create the folder the way Finder does, through the extension's create path.
        var folderMetadata = SendableItemMetadata(
            ocId: "folder-id", fileName: "folder", account: Self.account
        )
        folderMetadata.directory = true
        folderMetadata.classFile = NKTypeClassFile.directory.rawValue
        folderMetadata.serverUrl = Self.account.davFilesUrl

        let (createdFolderMaybe, createError) = await Item.create(
            basedOn: Item(
                metadata: folderMetadata,
                parentItemIdentifier: .rootContainer,
                account: Self.account,
                remoteInterface: remoteInterface,
                dbManager: Self.dbManager
            ),
            contents: nil,
            account: Self.account,
            remoteInterface: remoteInterface,
            progress: Progress(),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        XCTAssertNil(createError)
        let createdFolder = try XCTUnwrap(createdFolderMaybe)

        // The server-side creation inside that folder, with nothing in the subtree materialised.
        let remoteFolder = try XCTUnwrap(rootItem.children.first { $0.name == "folder" })
        let newFile = makeFile(name: "createdOnServer.txt", parent: remoteFolder, etag: "new-v1")

        let observer = try await runWorkingSetChanges(remoteInterface)

        // Assert the user-visible outcome first, so a regression fails on the symptom rather than on
        // the flag that causes it.
        XCTAssertNil(observer.error)
        XCTAssertTrue(
            reportedIds(observer).contains(newFile.identifier),
            "An item created on the server inside a locally created folder must be reported. Reported: \(reportedIds(observer).sorted())"
        )

        // Then the mechanism: creation must record the folder as visited, or the working-set scan has
        // no reason to read it.
        let storedFolder = try XCTUnwrap(
            Self.dbManager.itemMetadata(ocId: createdFolder.itemIdentifier.rawValue)
        )
        XCTAssertTrue(
            storedFolder.visitedDirectory,
            "A locally created folder must be recorded as visited; macOS will not enumerate it again."
        )
    }

    /// The eviction-shaped variant of the same hole: a folder Finder has enumerated whose contents are
    /// all dataless, where a new server-side child must still surface through the depth-1 read.
    func testItemCreatedOnServerInVisitedFolderWithNoMaterialisedContentIsReported() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let existingFile = makeFile(name: "itemA", parent: folder, etag: "itemA-v1")

        seed(folder, visitedDirectory: true) // enumerated once by Finder
        seed(existingFile, downloaded: false) // evicted / never downloaded

        // A new item appears on the server; the parent ETag is deliberately left alone, so discovery
        // cannot lean on the parent looking changed.
        let newFile = makeFile(name: "createdOnServer.txt", parent: folder, etag: "new-v1")

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertNil(observer.error)
        XCTAssertTrue(
            reportedIds(observer).contains(newFile.identifier),
            "An item created on the server inside a visited folder must be reported even when nothing in that folder is materialised."
        )
    }

    ///
    /// A trashed folder must not be scanned through the ordinary DAV path, whose 404 the scan would
    /// read as a deletion.
    ///
    func testTrashedFolderIsNotScannedThroughTheRegularDavPath() async throws {
        let liveFolder = makeFolder(name: "live", parent: rootItem, etag: "live-v1")
        seed(liveFolder, visitedDirectory: true)

        // A folder the user trashed: still visited, not soft-deleted, but living in the trashbin.
        var trashed = makeFolder(name: "gone", parent: rootItem, etag: "gone-v1")
            .toItemMetadata(account: Self.account)
        trashed.ocId = "trashed-folder"
        trashed.visitedDirectory = true
        trashed.deleted = false
        trashed.syncTime = oldSyncTime
        trashed.serverUrl = Self.account.trashUrl
        trashed.apply(fileName: "gone.d1788354069")
        Self.dbManager.addItemMetadata(trashed)
        XCTAssertTrue(trashed.isTrashed, "Precondition: the row must look trashed.")

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }

        let observer = try await runWorkingSetChanges(remoteInterface)
        let enumeratedPaths = recorder.paths

        XCTAssertNil(observer.error)
        XCTAssertTrue(
            enumeratedPaths.contains { $0.hasSuffix("/live") },
            "Sanity: the live materialised folder is still scanned."
        )
        XCTAssertFalse(
            enumeratedPaths.contains { $0.contains("/trashbin/") },
            "A trashed row must never be PROPFINDed through the regular DAV path. Got: \(enumeratedPaths)"
        )
        XCTAssertNotNil(
            Self.dbManager.itemMetadata(ocId: "trashed-folder"),
            "The trash row must survive the scan; trash reconciliation derives deletions from it."
        )
    }

    ///
    /// A directory's depth-1 read already returns its children's state, so unchanged children must
    /// not be PROPFINDed individually.
    ///
    func testUnchangedChildrenAreCoveredByTheirParentsRead() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let fileA = makeFile(name: "itemA", parent: folder, etag: "itemA-v1")
        let fileB = makeFile(name: "itemB", parent: folder, etag: "itemB-v1")

        // Folder visited and both files downloaded, so all three seed the scan, with nothing
        // changed on the server.
        seed(folder, visitedDirectory: true)
        seed(fileA, downloaded: true)
        seed(fileB, downloaded: true)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }

        let observer = try await runWorkingSetChanges(remoteInterface)
        let enumeratedPaths = recorder.paths

        XCTAssertNil(observer.error)
        XCTAssertTrue(
            enumeratedPaths.contains { $0.hasSuffix("/folder") },
            "The materialised folder is read at depth 1."
        )
        XCTAssertFalse(
            enumeratedPaths.contains { $0.hasSuffix("/folder/itemA") || $0.hasSuffix("/folder/itemB") },
            "Unchanged children are covered by the parent's depth-1 read and must not be read individually. Got: \(enumeratedPaths)"
        )
    }

    /// More folders at one depth than the read window holds must each be read once, and every change in them reported.
    func testWorkingSetScanReadsEveryFolderBeyondTheConcurrencyWindow() async throws {
        let names = (0 ..< 13).map { String(format: "f%02d", $0) }
        var newFiles = [MockRemoteItem]()

        for name in names {
            let folder = makeFolder(name: name, parent: rootItem, etag: "\(name)-v1")
            seed(folder, visitedDirectory: true)
            // Created on the server only, so the folder's read reports it as new.
            newFiles.append(makeFile(name: "new-in-\(name)", parent: folder, etag: "new-v1"))
        }

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }

        let observer = try await runWorkingSetChanges(remoteInterface)
        let enumeratedPaths = recorder.paths

        XCTAssertNil(observer.error)
        for name in names {
            XCTAssertEqual(enumeratedPaths.count(where: { $0.hasSuffix("/" + name) }), 1, "Folder \(name) must be read exactly once. Got: \(enumeratedPaths)")
        }
        let reported = reportedIds(observer)
        for file in newFiles {
            XCTAssertTrue(reported.contains(file.identifier), "\(file.name) must be reported.")
        }
        let finalAnchor = try XCTUnwrap(observer.finishes.last?.anchor)
        XCTAssertNotEqual(
            finalAnchor.rawValue,
            Enumerator.syncAnchor(at: anchorDate, storeIdentity: Self.dbManager.storeIdentity).rawValue,
            "A complete scan advances the sync anchor."
        )
    }

    /// Move the downloaded root-level file `f` into the visited root-level folder `A` on the mock server, both read in one wave, with `A`'s read held back so the 404 of `f` is merged first.
    private func moveFileIntoSiblingFolder(writingItsRowFirst: Bool) async throws -> (MockChangeObserver, MockRemoteItem) {
        let folder = makeFolder(name: "A", parent: rootItem, etag: "A-v1")
        let file = makeFile(name: "f", parent: rootItem, etag: "f-v1")
        seed(folder, visitedDirectory: true)
        seed(file, downloaded: true)

        rootItem.children.removeAll { $0 === file }
        file.parent = folder
        file.remotePath = folder.remotePath + "/f"
        folder.children.append(file)
        folder.versionIdentifier = "A-v2"

        var movedRow = file.toItemMetadata(account: Self.account)
        movedRow.downloaded = true
        movedRow.syncTime = oldSyncTime
        let movedRowToWrite = movedRow
        let dbManager = Self.dbManager

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            guard remotePath.hasSuffix("/A") else { return }

            if writingItsRowFirst {
                // Another enumeration of the folder recorded the move after the scan took its snapshot.
                dbManager.addItemMetadata(movedRowToWrite)
            }

            Thread.sleep(forTimeInterval: 0.3)
        }

        return try await (runWorkingSetChanges(remoteInterface), file)
    }

    func testFileMovedIntoASiblingFolderIsReportedAndNotDeleted() async throws {
        let (observer, file) = try await moveFileIntoSiblingFolder(writingItsRowFirst: false)

        XCTAssertNil(observer.error)
        XCTAssertTrue(reportedIds(observer).contains(file.identifier), "The moved file must be reported at its new location.")
        XCTAssertFalse(observer.deletedItemIdentifiers.map(\.rawValue).contains(file.identifier), "A file that moved on the server must not be reported deleted.")
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: file.identifier)?.deleted, false)
    }

    /// The listing finds the moved file unchanged because its row already moved, so only that listing shows the file still exists.
    func testFileMovedIntoASiblingFolderIsNotDeletedWhenItsRowAlreadyMoved() async throws {
        let (observer, file) = try await moveFileIntoSiblingFolder(writingItsRowFirst: true)

        XCTAssertNil(observer.error)
        XCTAssertFalse(observer.deletedItemIdentifiers.map(\.rawValue).contains(file.identifier), "A file that a listing in this scan returned must not be reported deleted.")
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: file.identifier)?.deleted, false)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: file.identifier)?.serverUrl, Self.account.davFilesUrl + "/A")
    }

    // MARK: - Push-targeted scanning

    func testPushedFileIdsResolveToTheItemsToRead() {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        folder.identifier = "1000"
        let file = makeFile(name: "file", parent: folder, etag: "file-v1")
        file.identifier = "1001"
        let rootFile = makeFile(name: "rootFile", parent: rootItem, etag: "rootFile-v1")
        rootFile.identifier = "1002"
        let deletedFile = makeFile(name: "deletedFile", parent: folder, etag: "deletedFile-v1")
        deletedFile.identifier = "1003"
        let unknownFolder = makeFolder(name: "unknown", parent: rootItem, etag: "unknown-v1")
        let orphan = makeFile(name: "orphan", parent: unknownFolder, etag: "orphan-v1")
        orphan.identifier = "1004"
        seed(folder, visitedDirectory: true)
        seed(file, downloaded: true)
        seed(rootFile, downloaded: true)
        var deletedMetadata = deletedFile.toItemMetadata(account: Self.account)
        deletedMetadata.deleted = true
        Self.dbManager.addItemMetadata(deletedMetadata)
        seed(orphan, downloaded: true)
        var renamedId = makeFile(name: "renamedId", parent: folder, etag: "renamedId-v1").toItemMetadata(account: Self.account)
        renamedId.ocId = "00001005oc"
        renamedId.fileId = "1005"
        Self.dbManager.addItemMetadata(renamedId)

        XCTAssertEqual(Self.dbManager.targetsForPushedFileIds(["1001"]), [NSFileProviderItemIdentifier("1001"), NSFileProviderItemIdentifier("1000")])
        XCTAssertEqual(Self.dbManager.targetsForPushedFileIds(["1000"]), [NSFileProviderItemIdentifier("1000")])
        XCTAssertEqual(Self.dbManager.targetsForPushedFileIds(["1002"]), [NSFileProviderItemIdentifier("1002"), .rootContainer])
        XCTAssertEqual(Self.dbManager.targetsForPushedFileIds(["1000", "9001"]), [NSFileProviderItemIdentifier("1000")])
        XCTAssertEqual(Self.dbManager.targetsForPushedFileIds(["9001"]), [])
        XCTAssertEqual(Self.dbManager.targetsForPushedFileIds(["1003"]), [NSFileProviderItemIdentifier("1000")])
        XCTAssertEqual(Self.dbManager.targetsForPushedFileIds(["1004"]), [NSFileProviderItemIdentifier("1004")])
        XCTAssertEqual(Self.dbManager.targetsForPushedFileIds(["1005"]), [NSFileProviderItemIdentifier("00001005oc"), NSFileProviderItemIdentifier("1000")])
    }

    func testATargetedScanReadsOnlyTheTargetedContainer() async throws {
        let targeted = makeFolder(name: "targeted", parent: rootItem, etag: "targeted-v1")
        let untouched = makeFolder(name: "untouched", parent: rootItem, etag: "untouched-v1")
        seed(targeted, visitedDirectory: true)
        seed(untouched, visitedDirectory: true)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        _ = await enumerator.scanMaterialisedItemsForRemoteChanges(
            startingFrom: [NSFileProviderItemIdentifier(targeted.identifier)]
        )

        XCTAssertEqual(recorder.paths.filter { $0.hasSuffix("/targeted") }.count, 1)
        XCTAssertEqual(recorder.paths.count, 1, "Got: \(recorder.paths)")
    }

    func testATargetedScanReportsAChangeInTheTargetAndNoDeletionOutsideIt() async throws {
        let targeted = makeFolder(name: "targeted", parent: rootItem, etag: "targeted-v1")
        let untouched = makeFolder(name: "untouched", parent: rootItem, etag: "untouched-v1")
        let goneOnServer = makeFile(name: "gone", parent: untouched, etag: "gone-v1")
        seed(targeted, visitedDirectory: true)
        seed(untouched, visitedDirectory: true)
        seed(goneOnServer, downloaded: true)

        let created = makeFile(name: "created", parent: targeted, etag: "created-v1")
        targeted.versionIdentifier = "targeted-v2"
        untouched.children.removeAll()
        untouched.versionIdentifier = "untouched-v2"

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account, rootItem: rootItem),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let changes = await enumerator.scanMaterialisedItemsForRemoteChanges(
            startingFrom: [NSFileProviderItemIdentifier(targeted.identifier)]
        )

        XCTAssertTrue(changes.updated.contains { $0.ocId == created.identifier })
        XCTAssertFalse(changes.deleted.contains { $0.ocId == goneOnServer.identifier })
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: goneOnServer.identifier)?.deleted, false)
        XCTAssertFalse(changes.hadFailure)
    }

    func testWithNothingRestrictedTheScanReadsEveryMaterialisedContainer() async throws {
        let alpha = makeFolder(name: "alpha", parent: rootItem, etag: "alpha-v1")
        let beta = makeFolder(name: "beta", parent: rootItem, etag: "beta-v1")
        seed(alpha, visitedDirectory: true)
        seed(beta, visitedDirectory: true)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        _ = await enumerator.scanMaterialisedItemsForRemoteChanges(startingFrom: nil)

        XCTAssertTrue(recorder.paths.contains { $0.hasSuffix("/alpha") })
        XCTAssertTrue(recorder.paths.contains { $0.hasSuffix("/beta") })
    }

    func testTheFirstDerivationWalksEverythingEvenWhenAPushNamedAContainer() async throws {
        let targeted = makeFolder(name: "targeted", parent: rootItem, etag: "targeted-v1")
        let untouched = makeFolder(name: "untouched", parent: rootItem, etag: "untouched-v1")
        seed(targeted, visitedDirectory: true)
        seed(untouched, visitedDirectory: true)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }
        let targets = RemoteChangeTargets()
        targets.record(targets: [NSFileProviderItemIdentifier(targeted.identifier)])

        _ = try await runWorkingSetChanges(remoteInterface, remoteChangeTargets: targets)

        XCTAssertTrue(recorder.paths.contains { $0.hasSuffix("/untouched") }, "Got: \(recorder.paths)")
        targets.record(targets: [NSFileProviderItemIdentifier(targeted.identifier)])
        XCTAssertEqual(targets.takeScope(), .targets([NSFileProviderItemIdentifier(targeted.identifier)]))
    }

    func testADerivationAfterAFullWalkReadsOnlyThePushedContainer() async throws {
        let targeted = makeFolder(name: "targeted", parent: rootItem, etag: "targeted-v1")
        let untouched = makeFolder(name: "untouched", parent: rootItem, etag: "untouched-v1")
        seed(targeted, visitedDirectory: true)
        seed(untouched, visitedDirectory: true)
        let created = makeFile(name: "created", parent: targeted, etag: "created-v1")
        targeted.versionIdentifier = "targeted-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: Date())
        targets.record(targets: [NSFileProviderItemIdentifier(targeted.identifier)])

        let observer = try await runWorkingSetChanges(remoteInterface, remoteChangeTargets: targets)

        XCTAssertEqual(recorder.paths.count, 1, "Got: \(recorder.paths)")
        XCTAssertTrue(reportedIds(observer).contains(created.identifier))
    }

    func testATargetedDerivationDoesNotPostponeTheNextFullWalk() async throws {
        let targeted = makeFolder(name: "targeted", parent: rootItem, etag: "targeted-v1")
        seed(targeted, visitedDirectory: true)

        let secondsLeftInTheInterval: TimeInterval = 60
        let lastFullWalk = Date(timeIntervalSinceNow: -RemoteChangeTargets.fullWalkInterval + secondsLeftInTheInterval)
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: lastFullWalk)
        targets.record(targets: [NSFileProviderItemIdentifier(targeted.identifier)])

        _ = try await runWorkingSetChanges(MockRemoteInterface(account: Self.account, rootItem: rootItem), remoteChangeTargets: targets)

        targets.record(targets: [NSFileProviderItemIdentifier(targeted.identifier)])
        XCTAssertEqual(targets.takeScope(now: lastFullWalk.addingTimeInterval(RemoteChangeTargets.fullWalkInterval)), .fullWalk)
    }

    func testAFailedTargetedReadIsRetriedByTheNextDerivation() async throws {
        let targeted = makeFolder(name: "targeted", parent: rootItem, etag: "targeted-v1")
        seed(targeted, visitedDirectory: true)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.enumerateErrorBySuffix = ["/targeted": NKError(statusCode: 500, fallbackDescription: "Internal Server Error")]
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: Date())
        targets.record(targets: [NSFileProviderItemIdentifier(targeted.identifier)])

        _ = try await runWorkingSetChanges(remoteInterface, remoteChangeTargets: targets)

        XCTAssertEqual(targets.takeScope(), .targets([NSFileProviderItemIdentifier(targeted.identifier)]))
    }

    func testAFullWalkWithAFailedReadStillCompletesAndRetriesTheFailedItem() async throws {
        let failing = makeFolder(name: "failing", parent: rootItem, etag: "failing-v1")
        let healthy = makeFolder(name: "healthy", parent: rootItem, etag: "healthy-v1")
        seed(failing, visitedDirectory: true)
        seed(healthy, visitedDirectory: true)

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.enumerateErrorBySuffix = ["/failing": NKError(statusCode: 500, fallbackDescription: "Internal Server Error")]
        let targets = RemoteChangeTargets()

        _ = try await runWorkingSetChanges(remoteInterface, remoteChangeTargets: targets)

        XCTAssertEqual(targets.takeScope(), .targets([NSFileProviderItemIdentifier(failing.identifier)]))
    }

    func testAPushRecordsTheItemsItNames() async {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        folder.identifier = "1000"
        seed(folder, visitedDirectory: true)
        let fileProviderExtension = makeExtension()
        fileProviderExtension.dbManager = Self.dbManager
        fileProviderExtension.remoteChangeTargets.noteFullWalkCompleted(startedAt: Date())

        await processFileIdsChanged([1000, 9001], in: fileProviderExtension)

        XCTAssertEqual(fileProviderExtension.remoteChangeTargets.takeScope(), .targets([NSFileProviderItemIdentifier("1000")]))
    }

    func testAPushWithoutFileIdsRequestsAFullWalk() async {
        let fileProviderExtension = makeExtension()
        fileProviderExtension.dbManager = Self.dbManager
        fileProviderExtension.remoteChangeTargets.noteFullWalkCompleted(startedAt: Date())
        fileProviderExtension.remoteChangeTargets.record(targets: [NSFileProviderItemIdentifier("1000")])

        await processFileIdsChanged([], in: fileProviderExtension)

        XCTAssertEqual(fileProviderExtension.remoteChangeTargets.takeScope(), .fullWalk)
    }

    func testAPushBeforeTheDatabaseIsReadyRequestsAFullWalk() async {
        let fileProviderExtension = makeExtension()
        fileProviderExtension.remoteChangeTargets.noteFullWalkCompleted(startedAt: Date())
        fileProviderExtension.remoteChangeTargets.record(targets: [NSFileProviderItemIdentifier("1000")])

        await processFileIdsChanged([1000], in: fileProviderExtension)

        XCTAssertEqual(fileProviderExtension.remoteChangeTargets.takeScope(), .fullWalk)
    }

    func testATargetedScanReadsAChangedVisitedChildThatHoldsNothingMaterialised() async throws {
        let parent = makeFolder(name: "parent", parent: rootItem, etag: "parent-v1")
        let child = makeFolder(name: "child", parent: parent, etag: "child-v1")
        seed(parent, visitedDirectory: true)
        seed(child, visitedDirectory: true)
        let created = makeFile(name: "created", parent: child, etag: "created-v1")
        child.versionIdentifier = "child-v2"
        parent.versionIdentifier = "parent-v2"

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account, rootItem: rootItem),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let changes = await enumerator.scanMaterialisedItemsForRemoteChanges(
            startingFrom: [NSFileProviderItemIdentifier(parent.identifier)]
        )

        XCTAssertTrue(changes.updated.contains { $0.ocId == created.identifier })
    }

    func testATargetedScanOfTheRootReadsTheRoot() async throws {
        seed(rootItem, visitedDirectory: true)
        let created = makeFile(name: "created", parent: rootItem, etag: "created-v1")
        rootItem.versionIdentifier = "root-v2"

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account, rootItem: rootItem),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let changes = await enumerator.scanMaterialisedItemsForRemoteChanges(startingFrom: [.rootContainer])

        XCTAssertTrue(changes.updated.contains { $0.ocId == created.identifier })
    }

    func testATargetedScanReadsADownloadedFileWhoseParentIsNotVisited() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let file = makeFile(name: "file", parent: folder, etag: "file-v1")
        seed(folder)
        seed(file, downloaded: true)
        file.versionIdentifier = "file-v2"
        file.modificationDate = Date()

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let recorder = EnumeratePathRecorder()
        remoteInterface.enumerateCallHandler = { remotePath, _, _, _, _, _, _, _ in
            recorder.add(remotePath)
        }
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let targets = try XCTUnwrap(Self.dbManager.targetsForPushedFileIds([file.identifier]))
        let changes = await enumerator.scanMaterialisedItemsForRemoteChanges(startingFrom: targets)

        XCTAssertTrue(changes.updated.contains { $0.ocId == file.identifier })
        XCTAssertEqual(recorder.paths.map { $0.hasSuffix("/folder") }, [true])
    }

    func testTheExtensionHandsItsTargetsToTheWorkingSetEnumerator() throws {
        let fileProviderExtension = makeExtension()
        fileProviderExtension.ncAccount = Self.account
        fileProviderExtension.dbManager = Self.dbManager

        let enumerator = try XCTUnwrap(fileProviderExtension.enumerator(for: .workingSet, request: NSFileProviderRequest()) as? Enumerator)

        XCTAssertTrue(enumerator.remoteChangeTargets === fileProviderExtension.remoteChangeTargets)
    }

    func testAPushNamingOnlyUnknownIdsRecordsNothing() async {
        let fileProviderExtension = makeExtension()
        fileProviderExtension.dbManager = Self.dbManager
        fileProviderExtension.remoteChangeTargets.noteFullWalkCompleted(startedAt: Date())

        await processFileIdsChanged([9001], in: fileProviderExtension)
        fileProviderExtension.remoteChangeTargets.record(targets: [NSFileProviderItemIdentifier("other")])

        XCTAssertEqual(fileProviderExtension.remoteChangeTargets.takeScope(), .targets([NSFileProviderItemIdentifier("other")]))
    }

    func testTheExtensionsOwnSignalRequestsAFullWalk() {
        let fileProviderExtension = makeExtension()
        fileProviderExtension.remoteChangeTargets.noteFullWalkCompleted(startedAt: Date())
        fileProviderExtension.remoteChangeTargets.record(targets: [NSFileProviderItemIdentifier("other")])

        fileProviderExtension.signalEnumerator { _ in }

        XCTAssertEqual(fileProviderExtension.remoteChangeTargets.takeScope(), .fullWalk)
    }

    func testATargetedScanKeepsAFileRenamedInAFolderThatIsNotVisited() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let file = makeFile(name: "before", parent: folder, etag: "file-v1")
        seed(rootItem, visitedDirectory: true)
        seed(folder)
        seed(file, downloaded: true)
        file.name = "after"
        file.remotePath = folder.remotePath + "/after"
        folder.versionIdentifier = "folder-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let targets = try XCTUnwrap(Self.dbManager.targetsForPushedFileIds([file.identifier]))
        let changes = await enumerator.scanMaterialisedItemsForRemoteChanges(startingFrom: targets)

        XCTAssertFalse(changes.deleted.contains { $0.ocId == file.identifier })
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: file.identifier)?.deleted, false)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: file.identifier)?.fileName, "after")
        XCTAssertEqual(remoteInterface.readOperationCount, 1)
    }

    func testATargetedScanReadsTheMaterialisedItemsBelowAFolderThatIsGone() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let subfolder = makeFolder(name: "subfolder", parent: folder, etag: "subfolder-v1")
        let file = makeFile(name: "file", parent: subfolder, etag: "file-v1")
        seed(rootItem, visitedDirectory: true)
        seed(folder, visitedDirectory: true)
        seed(subfolder, visitedDirectory: true)
        seed(file, downloaded: true)
        rootItem.children.removeAll()

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account, rootItem: rootItem),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let changes = await enumerator.scanMaterialisedItemsForRemoteChanges(startingFrom: [NSFileProviderItemIdentifier(folder.identifier), .rootContainer])

        XCTAssertTrue(changes.deleted.contains { $0.ocId == folder.identifier })
        XCTAssertTrue(changes.deleted.contains { $0.ocId == subfolder.identifier })
        XCTAssertTrue(changes.deleted.contains { $0.ocId == file.identifier })
        XCTAssertFalse(changes.needsFullWalk)
    }

    func testATargetedScanLeavesAMissingFolderToAFullWalkWhenItsParentWasNotRead() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let file = makeFile(name: "file", parent: folder, etag: "file-v1")
        seed(rootItem, visitedDirectory: true)
        seed(folder, visitedDirectory: true)
        seed(file, downloaded: true)
        rename(folder, to: "renamed")

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account, rootItem: rootItem),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let changes = await enumerator.scanMaterialisedItemsForRemoteChanges(startingFrom: [NSFileProviderItemIdentifier(folder.identifier)])

        XCTAssertTrue(changes.needsFullWalk)
        XCTAssertTrue(changes.deleted.isEmpty)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: folder.identifier)?.deleted, false)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: file.identifier)?.deleted, false)
    }

    func testADerivationKeepsAFolderRenamedWhileItsPushWasLost() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let file = makeFile(name: "file", parent: folder, etag: "file-v1")
        seed(rootItem, visitedDirectory: true)
        seed(folder, visitedDirectory: true)
        seed(file, downloaded: true)
        rename(folder, to: "renamed")
        rootItem.versionIdentifier = "root-v2"
        let secondsLeftInTheInterval: TimeInterval = 60
        let lastFullWalk = Date(timeIntervalSinceNow: -RemoteChangeTargets.fullWalkInterval + secondsLeftInTheInterval)
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: lastFullWalk)
        targets.record(targets: [NSFileProviderItemIdentifier(folder.identifier)])

        let observer = try await runWorkingSetChanges(MockRemoteInterface(account: Self.account, rootItem: rootItem), remoteChangeTargets: targets)

        targets.record(targets: [NSFileProviderItemIdentifier(folder.identifier)])
        XCTAssertEqual(targets.takeScope(now: lastFullWalk.addingTimeInterval(RemoteChangeTargets.fullWalkInterval)), .targets([NSFileProviderItemIdentifier(folder.identifier)]))

        XCTAssertFalse(observer.deletedItemIdentifiers.contains(NSFileProviderItemIdentifier(folder.identifier)))
        XCTAssertFalse(observer.deletedItemIdentifiers.contains(NSFileProviderItemIdentifier(file.identifier)))
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: folder.identifier)?.fileName, "renamed")
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: file.identifier)?.deleted, false)
    }

    func testADerivationKeepsARenamedFolderWhoseOldPathHoldsANewFolder() async throws {
        let folder = makeFolder(name: "Reports", parent: rootItem, etag: "folder-v1")
        let file = makeFile(name: "file", parent: folder, etag: "file-v1")
        seed(rootItem, visitedDirectory: true)
        seed(folder, visitedDirectory: true)
        seed(file, downloaded: true)
        rename(folder, to: "Reports 2025")
        let newFolder = makeFolder(name: "Reports", parent: rootItem, etag: "new-v1")
        newFolder.identifier = "new-reports"
        rootItem.versionIdentifier = "root-v2"
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: Date())
        targets.record(targets: [NSFileProviderItemIdentifier(folder.identifier)])

        let observer = try await runWorkingSetChanges(MockRemoteInterface(account: Self.account, rootItem: rootItem), remoteChangeTargets: targets)

        XCTAssertFalse(observer.deletedItemIdentifiers.contains(NSFileProviderItemIdentifier(folder.identifier)))
        XCTAssertFalse(observer.deletedItemIdentifiers.contains(NSFileProviderItemIdentifier(file.identifier)))
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: folder.identifier)?.fileName, "Reports 2025")
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: file.identifier)?.deleted, false)
    }

    func testTheFullWalkOverrulesATargetedDeletionOfAnItemThatMoved() async throws {
        let source = makeFolder(name: "source", parent: rootItem, etag: "source-v1")
        let destination = makeFolder(name: "destination", parent: rootItem, etag: "destination-v1")
        let unreadParent = makeFolder(name: "unread", parent: rootItem, etag: "unread-v1")
        let movedFolder = makeFolder(name: "moved", parent: unreadParent, etag: "moved-v1")
        let movedFile = makeFile(name: "file", parent: source, etag: "file-v1")
        seed(rootItem, visitedDirectory: true)
        seed(source, visitedDirectory: true)
        seed(destination, visitedDirectory: true)
        seed(unreadParent, visitedDirectory: true)
        seed(movedFolder, visitedDirectory: true)
        seed(movedFile, downloaded: true)
        move(movedFile, to: destination)
        move(movedFolder, to: destination)
        for folder in try [XCTUnwrap(rootItem), source, destination, unreadParent] {
            folder.versionIdentifier += "-v2"
        }
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: Date())
        targets.record(targets: [NSFileProviderItemIdentifier(source.identifier), NSFileProviderItemIdentifier(movedFolder.identifier)])

        let observer = try await runWorkingSetChanges(MockRemoteInterface(account: Self.account, rootItem: rootItem), remoteChangeTargets: targets)

        XCTAssertFalse(observer.deletedItemIdentifiers.contains(NSFileProviderItemIdentifier(movedFile.identifier)))
        XCTAssertFalse(observer.deletedItemIdentifiers.contains(NSFileProviderItemIdentifier(movedFolder.identifier)))
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: movedFile.identifier)?.deleted, false)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: movedFile.identifier)?.serverUrl, destination.remotePath)
    }

    private func move(_ item: MockRemoteItem, to newParent: MockRemoteItem) {
        item.parent?.children.removeAll { $0 === item }
        let oldPath = item.remotePath
        let newPath = newParent.remotePath + "/" + item.name
        var pending = [item]

        while let current = pending.popLast() {
            current.remotePath = newPath + current.remotePath.dropFirst(oldPath.count)
            pending += current.children
        }

        item.parent = newParent
        newParent.children.append(item)
    }

    func testATargetedScanReadsAPinnedFolderThatIsNotVisited() async throws {
        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        seed(folder, keepDownloaded: true)
        let created = makeFile(name: "created", parent: folder, etag: "created-v1")
        folder.versionIdentifier = "folder-v2"

        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: MockRemoteInterface(account: Self.account, rootItem: rootItem),
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let changes = await enumerator.scanMaterialisedItemsForRemoteChanges(startingFrom: [NSFileProviderItemIdentifier(folder.identifier)])

        XCTAssertTrue(changes.updated.contains { $0.ocId == created.identifier })
    }

    private func rename(_ item: MockRemoteItem, to name: String) {
        let oldPath = item.remotePath
        let newPath = (oldPath as NSString).deletingLastPathComponent + "/" + name
        var pending = [item]

        while let current = pending.popLast() {
            current.remotePath = newPath + current.remotePath.dropFirst(oldPath.count)
            pending += current.children
        }

        item.name = name
    }

    private func makeExtension() -> FileProviderExtension {
        FileProviderExtension(domain: NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(UUID().uuidString), displayName: "Test"))
    }

    private func processFileIdsChanged(_ fileIds: [NSNumber], in fileProviderExtension: FileProviderExtension) async {
        await withCheckedContinuation { continuation in
            fileProviderExtension.processFileIdsChanged(fileIds) { _ in
                continuation.resume()
            }
        }
    }
}
