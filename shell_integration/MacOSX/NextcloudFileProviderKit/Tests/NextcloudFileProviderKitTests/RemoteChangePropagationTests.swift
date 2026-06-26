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
//        -> checkMaterializedItemsOnServer()        // PROPFINDs every *materialized* item,
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
//    - S1/S2: `checkMaterializedItemsOnServer` now returns the changes it discovers (reported
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
        // stable-33.0 uses a plain ISO8601 sync anchor (the version-tagged anchor from #10065 is not
        // on this branch), so build the anchor the same way the enumerator and the other tests do.
        let anchor = try NSFileProviderSyncAnchor(
            XCTUnwrap(ISO8601DateFormatter().string(from: anchorDate).data(using: .utf8))
        )
        try await observer.enumerateChanges(from: anchor)
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
    /// The change is written to the DB by `checkMaterializedItemsOnServer` but
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
        // checkMaterializedItemsOnServer returns it directly, rather than being gated on the
        // parent folder's syncTime via pendingWorkingSetChanges.
        XCTAssertTrue(
            reportedIds(observer).contains(file.identifier),
            "A changed non-materialized child should surface even if the parent folder ETag did not change."
        )
    }

    /// Scenario C (S1 headline — bounded recursion): a change to a grandchild file under a NON-visited
    /// subdirectory of a visited folder. `checkMaterializedItemsOnServer` reads the visited folder
    /// only at depth 1, so it sees the changed subdirectory but not the grandchild. The fix enqueues
    /// the changed subdirectory and scans it — BUT only because the subtree contains a materialized
    /// item (a downloaded sibling `anchor`), which is what makes the working set care about it. So the
    /// grandchild's change is discovered, persisted and reported rather than dropped behind the
    /// depth-1 read. The companion test below proves the complementary bound: a changed subtree with
    /// NOTHING materialized inside is deliberately NOT crawled.
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
        // Regression guard (S1 headline): checkMaterializedItemsOnServer enqueues the changed subfolder
        // (its subtree holds a materialized item) and scans it, so the grandchild's change surfaces.
        XCTAssertTrue(
            reportedIds(observer).contains(grandchild.identifier),
            "A change to a grandchild under a non-visited subfolder with a materialized descendant should surface."
        )
    }

    /// Scenario D (PROPFIND-storm guard): a sibling subtree that the server reports as changed (its
    /// ETag bubbled up) but which the user NEVER opened — nothing inside it is materialized, and its
    /// children are brand new on the server. `checkMaterializedItemsOnServer` must report the sibling
    /// directory's own change but must NOT recurse into it; otherwise a single working-set signal on a
    /// freshly activated / sparse domain walks entire never-visited subtrees (observed: ~1700 PROPFINDs
    /// to depth 7 from one push). Verifies no enumerate call is ever issued into the sibling subtree
    /// and that its never-enumerated descendants are not reported.
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
}
