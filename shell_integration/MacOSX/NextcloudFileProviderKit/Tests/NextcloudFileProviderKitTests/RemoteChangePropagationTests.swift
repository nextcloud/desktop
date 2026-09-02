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
//        -> scanMaterialisedItemsForRemoteChanges()  // PROPFINDs every *materialized* item,
//                                                    // writes fresh metadata + syncTime to the DB
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
import RealmSwift
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

    static let dbManager = FilesDatabaseManager(
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
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = name
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
        syncTime: Date? = nil
    ) -> SendableItemMetadata {
        var metadata = item.toItemMetadata(account: Self.account)
        metadata.downloaded = downloaded
        metadata.visitedDirectory = visitedDirectory
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
        _ remoteInterface: MockRemoteInterface
    ) async throws -> MockChangeObserver {
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: anchorDate))
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
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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
        let realm = Self.dbManager.ncDatabase()
        let ghost = RealmItemMetadata()
        ghost.ocId = "staleGhost"
        ghost.account = Self.account.ncKitAccount
        ghost.updateLocation(serverUrl: storedLive.serverUrl, fileName: storedLive.fileName)
        ghost.uploaded = true
        ghost.syncTime = oldSyncTime
        try realm.write { realm.add(ghost) }

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

    /// Scenario A (S1/S2 control): a NON-materialized file changed inside a visited (materialized)
    /// folder, AND the folder's ETag is bumped too (a well-behaved server propagates child changes
    /// up to the parent's ETag). Expected to PASS — the parent's bumped `syncTime` lets the child
    /// scan in `pendingWorkingSetChanges` pick up the changed child.
    func testChangedChildSurfacesWhenParentEtagAlsoBumped() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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

    /// S4: the change-detection predicate `isInSameDatabaseStoreableRemoteState` keys on ETag (+ a
    /// fixed field set). If a file's content changes but its ETag is unchanged, the predicate treats
    /// it as unchanged and the update is skipped. Real servers bump the ETag on content change, so
    /// this documents the predicate's reliance on a correct ETag rather than a likely field bug.
    func testSameEtagContentChangeIsReported() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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
    /// downloaded file; the FIRST-scanned folder's PROPFIND is forced to fail with a non-404 server
    /// error (standing in for a large container timing out — the position the account root occupies in
    /// production, since the scan queue is sorted parent-first). The change in the OTHER folder must
    /// still be discovered and reported. Before the fix `scanMaterialisedItemsForRemoteChanges` `break`ed
    /// on the first failure and silently dropped every later folder's changes (the "files uploaded on
    /// the web never appear" bug). The incomplete scan must also NOT advance the working-set sync point,
    /// so the next signal re-derives and can pick up the folder it could not read this pass.
    func testWorkingSetScanContinuesPastAFailedFolderRead() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

        // "a" sorts before "zzzzzzzzzz" by remote-path length, so the failing folder is scanned first.
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
        // Force the first-scanned folder's depth-1 read to fail with a non-404 error (not a deletion).
        remoteInterface.enumerateErrorBySuffix = [
            "/a": NKError(statusCode: 500, fallbackDescription: "Internal Server Error")
        ]

        let inputAnchor = Enumerator.syncAnchor(at: anchorDate)
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
        // …and the change in the folder scanned AFTER the failing one still surfaces — the core regression.
        XCTAssertTrue(
            reportedIds(observer).contains(fileInHealthy.identifier),
            "A read failure on one folder must not prevent a later folder's change from being reported."
        )
        // The incomplete scan must keep the incoming sync anchor rather than advancing to a fresh "now",
        // so the framework does not treat the domain as fully synced past the folder it could not read.
        let finalAnchor = try XCTUnwrap(observer.finishes.last?.anchor)
        XCTAssertEqual(
            finalAnchor.rawValue, inputAnchor.rawValue,
            "An incomplete working-set scan must not advance the working-set sync anchor."
        )
    }

    /// S3 (push gate): on notify_push servers `processFileIdsChanged` signals the working set when at
    /// least one received fileId is locally known (`containsAnyItemMetadata`). The server propagates
    /// a change's ETag up every ancestor to the user's root, so a `notify_file_id` for a newly-created
    /// item also carries its PARENT folder's fileId (and ancestors) — all of which the client has
    /// enumerated. The gate therefore matches and the new item triggers a refresh; it only ignores a
    /// push whose ids are entirely outside the enumerated tree. See nextcloud/desktop#6430.
    func testPushGateMatchesParentFolderOfNewItem() {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        // `MockRemoteItem.identifier` becomes both ocId and fileId; use a numeric id like the server.
        folder.identifier = "1234"
        seed(folder, visitedDirectory: true)

        // A new child of folder 1234 has an id the client has never seen, but the server's parent-etag
        // propagation means the push also carries the known parent folder id 1234 — so the gate matches
        // and the new child is not dropped.
        XCTAssertTrue(
            Self.dbManager.containsAnyItemMetadata(fileIds: ["1234", "9001"]),
            "A push carrying the (known) parent folder id must pass the gate so the new child surfaces."
        )
        // A push whose ids are all outside the enumerated tree affects nothing the working set tracks.
        XCTAssertFalse(
            Self.dbManager.containsAnyItemMetadata(fileIds: ["9001", "9002"]),
            "A push with no locally known ids is correctly ignored."
        )
    }

    /// Headline regression for nextcloud/desktop#9688 and #10681: a folder the user creates on the Mac,
    /// then an item created inside it on the server (web UI, public upload link, another user).
    ///
    /// The folder is created through the real `Item.create` path rather than `seed(...)`, because the bug
    /// was in what creation writes to the database: it set `downloaded` but not `visitedDirectory`, and
    /// only `visitedDirectory` puts a directory into the materialised set that
    /// `scanMaterialisedItemsForRemoteChanges()` reads. The folder was therefore never PROPFINDed by the
    /// working-set scan, and the parent's depth-1 read would not enqueue it either — a changed child
    /// directory is only crawled when its subtree holds something materialised, and a brand-new remote
    /// item is not downloaded. macOS never re-enumerates a container the extension created for it, so
    /// nothing repaired the gap: the new item stayed invisible until the folder was forced offline
    /// ("Keep offline"), which is exactly the workaround both issues report.
    func testItemCreatedOnServerInLocallyCreatedFolderIsReported() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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

        // The server-side creation inside that folder. Nothing in the subtree is materialised.
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

    /// The eviction-shaped variant of the same hole: a folder Finder has enumerated (so macOS will not
    /// enumerate it again) whose contents are all dataless, and a NEW item appears inside it on the
    /// server. `visitedDirectory` is deliberately kept for directories macOS reports as dataless (see
    /// `MaterializedEnumerationObserver`), so such a folder stays in the materialised set and is read at
    /// depth 1 — which is what must surface the new child. Guards that property against a future
    /// narrowing of the materialised-set query or of the scan's seed set.
    func testItemCreatedOnServerInVisitedFolderWithNoMaterialisedContentIsReported() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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
    /// A trashed folder must not be scanned through the ordinary DAV path.
    ///
    /// Trashing rewrites `serverUrl` to the trashbin but leaves `deleted == false`, and a folder
    /// keeps `visitedDirectory`, so the row stayed in the materialised set. The scan then PROPFINDed
    /// `/remote.php/dav/trashbin/<user>/trash/<name>.dNNNN`, which 404s — and the scan reads a 404
    /// as "the item is gone", reporting it deleted and hard-removing the very row the trash
    /// reconciliation derives permanent deletions from.
    ///
    func testTrashedFolderIsNotScannedThroughTheRegularDavPath() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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
    /// This is the coverage rule the scan's ordering exists to serve, and the one thing the
    /// concurrent (depth-stratified) walk must not lose: a parent is always one level shallower than
    /// the children it covers, so it is always read in an earlier wave. Batch the walk by anything
    /// other than depth and a folder plus its files land in the same wave, every file is read
    /// separately, and the read count multiplies.
    ///
    func testUnchangedChildrenAreCoveredByTheirParentsRead() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let fileA = makeFile(name: "itemA", parent: folder, etag: "itemA-v1")
        let fileB = makeFile(name: "itemB", parent: folder, etag: "itemB-v1")

        // Folder visited, both files downloaded: all three are in the materialised set and so all
        // three seed the scan. Nothing has changed on the server.
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

    // MARK: - Push-targeted scanning

    /// A push names what changed; the client must turn that into where to look. A changed file
    /// resolves to its parent container, a changed directory to itself.
    func testPushedFileIdsResolveToContainersToReRead() {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        folder.identifier = "1000"
        let file = makeFile(name: "itemA", parent: folder, etag: "itemA-v1")
        file.identifier = "1001"
        seed(folder, visitedDirectory: true)
        seed(file, downloaded: true)

        // The file resolves to its parent folder: a depth-1 read of the parent shows the file's new
        // state, or its absence.
        let forFile = Self.dbManager.containersForPushedFileIds(["1001"])
        XCTAssertFalse(
            forFile.contains(NSFileProviderItemIdentifier(file.identifier)),
            "The file itself is not a container to read."
        )
        XCTAssertTrue(
            forFile.contains(NSFileProviderItemIdentifier(folder.identifier)),
            "A changed file must resolve to its parent container. Got: \(forFile)"
        )

        // A directory resolves to itself.
        XCTAssertTrue(
            Self.dbManager.containersForPushedFileIds(["1000"])
                .contains(NSFileProviderItemIdentifier(folder.identifier))
        )

        // Ids outside the enumerated tree contribute nothing.
        XCTAssertTrue(Self.dbManager.containersForPushedFileIds(["9001"]).isEmpty)
    }

    /// The point of the whole change: a push naming one folder must read that folder, not the entire
    /// materialised set.
    func testATargetedScanReadsOnlyTheTargetedContainer() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

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
            restrictedToContainers: [NSFileProviderItemIdentifier(targeted.identifier)]
        )

        let paths = recorder.paths
        XCTAssertTrue(paths.contains { $0.hasSuffix("/targeted") }, "The targeted container is read.")
        XCTAssertFalse(
            paths.contains { $0.hasSuffix("/untouched") },
            "An untargeted container must not be read. Got: \(paths)"
        )
    }

    /// Push is an accelerator, not a replacement: with nothing targeted the walk must still be full,
    /// or anything push dropped would never reconcile.
    func testWithNothingTargetedTheScanIsStillFull() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

        let a = makeFolder(name: "alpha", parent: rootItem, etag: "alpha-v1")
        let b = makeFolder(name: "beta", parent: rootItem, etag: "beta-v1")
        seed(a, visitedDirectory: true)
        seed(b, visitedDirectory: true)

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
        _ = await enumerator.scanMaterialisedItemsForRemoteChanges(restrictedToContainers: nil)

        let paths = recorder.paths
        XCTAssertTrue(paths.contains { $0.hasSuffix("/alpha") })
        XCTAssertTrue(paths.contains { $0.hasSuffix("/beta") }, "A full scan reads every materialised container.")
    }

    /// A steady stream of pushes must not postpone reconciliation forever.
    func testAFullScanIsForcedOnceTheIntervalElapses() {
        let targets = RemoteChangeTargets.shared

        XCTAssertTrue(targets.shouldRunFullScan(), "Before any full scan has run, one is due.")

        let completedAt = Date()
        targets.noteFullScanCompleted(at: completedAt)
        XCTAssertFalse(targets.shouldRunFullScan(now: completedAt.addingTimeInterval(60)))
        XCTAssertTrue(
            targets.shouldRunFullScan(now: completedAt.addingTimeInterval(RemoteChangeTargets.fullScanInterval + 1)),
            "Once the interval has elapsed a full reconciliation is due again."
        )
    }

    /// Consuming clears, so one push's containers are not re-scanned on every later derivation.
    func testConsumingTargetsClearsThem() {
        let targets = RemoteChangeTargets.shared
        XCTAssertNil(targets.consumeTargets(), "Nothing recorded means no targeting information.")

        targets.record(containers: [NSFileProviderItemIdentifier("a"), NSFileProviderItemIdentifier("b")])
        XCTAssertEqual(targets.consumeTargets()?.count, 2)
        XCTAssertNil(targets.consumeTargets(), "A second derivation must not reuse consumed targets.")
    }
}
