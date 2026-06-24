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

    /// Scenario C (S1 headline — depth-1 limitation): a change to a grandchild file that lives under
    /// a NON-materialized subdirectory of a visited folder. `checkMaterializedItemsOnServer` reads
    /// the visited folder only at depth 1, so it sees the changed subdirectory but never reads the
    /// grandchild; the grandchild's `syncTime` is never advanced and it is never reported — even
    /// with a perfectly well-behaved server that bumped every ancestor ETag. (Reachable when a prior
    /// recursive enumeration populated the grandchild while the subdirectory was never opened.)
    /// Asserts the desired behaviour; a FAILURE localizes the depth-1 drop.
    func testDeepChangeUnderNonMaterializedSubfolderIsReported() async throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let childFolder = makeFolder(name: "childFolder", parent: folder, etag: "child-v1")
        let grandchild = makeFile(name: "itemX", parent: childFolder, etag: "itemX-v1")

        seed(folder, visitedDirectory: true) // materialized
        seed(childFolder, visitedDirectory: false) // NOT materialized
        seed(grandchild, downloaded: false) // NOT materialized, but known in DB

        // Well-behaved server: every ancestor ETag bumped along with the grandchild's change.
        grandchild.versionIdentifier = "itemX-v2"
        grandchild.modificationDate = Date()
        childFolder.versionIdentifier = "child-v2"
        folder.versionIdentifier = "folder-v2"

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        let observer = try await runWorkingSetChanges(remoteInterface)

        XCTAssertNil(observer.error)
        // Regression guard (S1 headline): checkMaterializedItemsOnServer now enqueues the changed
        // subfolder and scans it, so the grandchild's change is discovered, persisted and reported
        // rather than dropped behind the depth-1 read of the visited folder.
        XCTAssertTrue(
            reportedIds(observer).contains(grandchild.identifier),
            "A change to a grandchild under a non-materialized subfolder should surface in the working set."
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

    /// S3 (push gate, DB boundary): on notify_push servers the extension's `processFileIdsChanged`
    /// signals the working set ONLY when at least one changed fileId is already in the local DB
    /// (`containsAnyItemMetadata`). A NEWLY-created remote file has a fileId the DB has never seen,
    /// so the notification is dropped and no enumeration is triggered. This characterizes the gate
    /// directly: a known fileId passes; an unknown one (the new-file case) does not.
    func testPushGateDropsUnknownFileId() throws {
        let db = Self.dbManager.ncDatabase(); debugPrint(db)

        let folder = makeFolder(name: "folder", parent: rootItem, etag: "folder-v1")
        let known = makeFile(name: "known", parent: folder, etag: "known-v1")
        // `MockRemoteItem.identifier` becomes both ocId and fileId; use a numeric id like the server.
        known.identifier = "100"
        seed(folder, visitedDirectory: true)
        seed(known, downloaded: true)

        XCTAssertTrue(
            Self.dbManager.containsAnyItemMetadata(fileIds: ["100"]),
            "A push for a known fileId must pass the gate so the working set is signalled."
        )
        XCTAssertFalse(
            Self.dbManager.containsAnyItemMetadata(fileIds: ["200"]),
            "A push for an unknown fileId (e.g. a newly-created remote file) is dropped by the gate — "
                + "the new file never triggers a working-set signal on push-only accounts."
        )
    }
}
