//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import Testing
@testable import TestInterface

///
/// Coverage for the working-set scan: which directories a pass lists, what it reports, and what it remembers.
///
/// Every test has its own database and mock server. Seeded rows carry a `syncTime` before the anchor, so only what a pass finds on the server is reported.
///
@Suite("Working-set scan")
struct WorkingSetScanTests {
    let account = DatabaseTestSuites.account
    let manager = DatabaseTestSuites.makeManager()
    let rootItem: MockRemoteItem
    let remoteInterface: MockRemoteInterface
    let recorder = EnumeratePathRecorder()
    let anchor: NSFileProviderSyncAnchor
    let oldSyncTime = Date(timeIntervalSinceNow: -600)

    init() {
        rootItem = MockRemoteItem.rootItem(account: account)
        remoteInterface = MockRemoteInterface(account: account, rootItem: rootItem)
        recorder.record(on: remoteInterface)
        anchor = Enumerator.syncAnchor(at: Date(timeIntervalSinceNow: -300), storeIdentity: manager.storeIdentity)
    }

    // MARK: - Fixtures

    private func makeFolder(_ name: String, in parent: MockRemoteItem) -> MockRemoteItem {
        let folder = MockRemoteItem(
            identifier: name, versionIdentifier: "\(name)-v1", name: name, remotePath: parent.remotePath + "/" + name, directory: true,
            account: account.ncKitAccount, username: account.username, userId: account.id, serverUrl: account.serverUrl
        )
        folder.parent = parent
        parent.children.append(folder)
        return folder
    }

    private func makeFile(_ name: String, in parent: MockRemoteItem) -> MockRemoteItem {
        let file = MockRemoteItem(
            identifier: name, versionIdentifier: "\(name)-v1", name: name, remotePath: parent.remotePath + "/" + name, data: Data([1, 2, 3]),
            account: account.ncKitAccount, username: account.username, userId: account.id, serverUrl: account.serverUrl
        )
        file.parent = parent
        parent.children.append(file)
        return file
    }

    /// Store the item as the server has it, with the given local flags and a `syncTime` before the anchor.
    @discardableResult
    private func seed(_ item: MockRemoteItem, downloaded: Bool = false, visited: Bool = false) -> SendableItemMetadata {
        var metadata = item.toItemMetadata(account: account)
        metadata.downloaded = downloaded
        metadata.visitedDirectory = visited
        metadata.syncTime = oldSyncTime
        manager.addItemMetadata(metadata)
        return metadata
    }

    /// Store the root and every item of the mock tree, visiting every directory.
    private func seedTree(visitingDirectories: Bool = true) {
        seed(rootItem, visited: visitingDirectories)
        var queue = rootItem.children
        while let item = queue.first {
            queue.removeFirst()
            seed(item, visited: visitingDirectories && item.directory)
            queue.append(contentsOf: item.children)
        }
    }

    private struct Pass {
        let observer: MockChangeObserver
        let paths: [String]
        var reported: Set<String> {
            Set(observer.changedItems.map(\.itemIdentifier.rawValue))
        }

        var deleted: Set<String> {
            Set(observer.deletedItemIdentifiers.map(\.rawValue))
        }

        var anchor: NSFileProviderSyncAnchor? {
            observer.finishes.last?.anchor
        }
    }

    /// One working-set change pass, recording the directories it lists.
    private func pass(from anchor: NSFileProviderSyncAnchor? = nil) async throws -> Pass {
        recorder.reset()
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet, account: account, remoteInterface: remoteInterface, dbManager: manager, log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges(from: anchor ?? self.anchor)
        return Pass(observer: observer, paths: recorder.paths.map { $0.replacingOccurrences(of: account.davFilesUrl, with: "") })
    }

    /// A pass which records the reconciled state of every directory, so the next pass is a warm one.
    private func warmUp() async throws -> NSFileProviderSyncAnchor {
        let warm = try await pass()
        #expect(warm.observer.error == nil)
        return try #require(warm.anchor)
    }

    /// A delivered batch is acknowledged right after the observer's finish, on the main actor, so its effect on the store can trail the pass by a moment.
    private func waitUntil(_ comment: Comment, _ condition: @escaping () -> Bool) async throws {
        for _ in 0 ..< 200 where !condition() {
            try await Task.sleep(for: .milliseconds(5))
        }
        #expect(condition(), comment)
    }

    /// Anchors carry whole seconds, so a pass which starts within the second of the given anchor could end on an equal one.
    private func waitForTheSecondAfter(_ anchor: NSFileProviderSyncAnchor) async throws {
        let anchorDate = try #require(Enumerator.parseSyncAnchor(anchor)?.date)
        let remaining = anchorDate.addingTimeInterval(1).timeIntervalSinceNow
        if remaining > 0 {
            try await Task.sleep(for: .seconds(remaining))
        }
    }

    // MARK: - Cost

    @Test func anIdleDomainCostsOneRequest() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let c = makeFolder("c", in: b)
        makeFile("f", in: c)
        seedTree()
        let warmAnchor = try await warmUp()
        #expect(manager.scannedRemoteStateForTesting(ocId: "b") != nil)

        let idle = try await pass(from: warmAnchor)

        #expect(idle.paths == [""], "Only the root is listed; its children are unchanged since the last pass.")
        #expect(idle.reported.isEmpty)
        #expect(idle.deleted.isEmpty)
    }

    @Test func theFirstPassListsEveryVisitedDirectoryOnce() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        makeFolder("c", in: b)
        seedTree()

        let cold = try await pass()

        #expect(cold.paths == ["", "/a", "/a/b", "/a/b/c"], "Nothing was reconciled yet, so every visited directory is listed, whatever its etag.")
        #expect(try manager.scannedRemoteStateForTesting(ocId: "c") == FilesDatabaseManager.scanRemoteState(of: #require(manager.itemMetadata(ocId: "c"))))
    }

    @Test func aChangedDeepFileCostsOnlyThePath() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let c = makeFolder("c", in: b)
        let f = makeFile("f", in: c)
        let sibling = makeFolder("sibling", in: rootItem)
        makeFile("other", in: sibling)
        seedTree(visitingDirectories: false)
        seed(rootItem, visited: true)
        seed(a, visited: true)
        seed(b, visited: true)
        seed(sibling, visited: true)
        seed(f, downloaded: true)
        let warmAnchor = try await warmUp()

        f.changeVersion(to: "f-v2")
        let changed = try await pass(from: warmAnchor)

        #expect(changed.paths == ["", "/a", "/a/b", "/a/b/c"], "The walk follows the changed etags down to the file's directory, which was never visited, and nowhere else.")
        #expect(changed.reported.contains("f"))
        #expect(!changed.reported.contains("other"))
        #expect(manager.itemMetadata(ocId: "f")?.etag == "f-v2")
    }

    @Test func aDownloadedFileUnderAnUnvisitedDirectoryIsNotReadOnItsOwn() async throws {
        let parent = makeFolder("parent", in: rootItem)
        let f = makeFile("f", in: parent)
        seed(rootItem, visited: true)
        seed(parent)
        seed(f, downloaded: true)
        let warmAnchor = try await warmUp()

        let idle = try await pass(from: warmAnchor)
        #expect(idle.paths == [""], "The file is covered by its parent's listing, which is unchanged.")

        f.changeVersion(to: "f-v2")
        let changed = try await pass(from: warmAnchor)
        #expect(changed.paths == ["", "/parent"])
        #expect(changed.reported.contains("f"))
    }

    @Test func aChangedDirectoryWithNothingMaterialisedBelowIsReportedNotListed() async throws {
        let empty = makeFolder("empty", in: rootItem)
        seed(rootItem, visited: true)
        seed(empty)
        let warmAnchor = try await warmUp()

        makeFile("new", in: empty)
        empty.bumpVersion()
        let changed = try await pass(from: warmAnchor)

        #expect(changed.paths == [""])
        #expect(changed.reported.contains("empty"), "The directory's own change is reported from the root listing.")
        #expect(!changed.reported.contains("new"), "Nothing inside it is tracked, so its contents are read when the user opens it.")
        #expect(manager.scannedRemoteStateForTesting(ocId: "empty") == nil)
    }

    @Test func nothingMaterialisedCostsNoRequest() async throws {
        makeFolder("a", in: rootItem)

        let pass = try await pass()

        #expect(pass.paths.isEmpty)
        #expect(pass.reported.isEmpty)
        #expect(pass.observer.error == nil)
    }

    // MARK: - Deletions and moves

    @Test func aDeletedSubtreeIsReportedWithoutPerItemReads() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let c = makeFolder("c", in: b)
        let f = makeFile("f", in: c)
        seed(rootItem, visited: true)
        seed(a, visited: true)
        seed(b, visited: true)
        seed(c)
        seed(f, downloaded: true)
        let warmAnchor = try await warmUp()

        rootItem.removeChild(a)
        let afterDelete = try await pass(from: warmAnchor)

        #expect(afterDelete.paths == [""], "The deletion is known from the root listing alone.")
        #expect(afterDelete.deleted == ["a", "b", "c", "f"])
        try await waitUntil("The acknowledged working-set deletion removes the rows.") {
            ["a", "b", "c", "f"].allSatisfy { manager.itemMetadata(ocId: $0) == nil }
        }
    }

    @Test func aDeletedSubtreeIsStampedSoALostBatchIsDerivedAgain() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let f = makeFile("f", in: b)
        seed(rootItem, visited: true)
        seed(a, visited: true)
        seed(b)
        seed(f, downloaded: true)
        _ = try await warmUp()

        rootItem.removeChild(a)
        // The scan alone, without delivery, is what a crash between the two leaves behind.
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet, account: account, remoteInterface: remoteInterface, dbManager: manager, log: FileProviderLogMock()
        )
        let scanned = await enumerator.scanMaterialisedItemsForRemoteChanges()

        #expect(Set(scanned.deleted.map(\.ocId)) == ["a", "b", "f"])
        for ocId in ["a", "b", "f"] {
            let row = try #require(manager.itemMetadata(ocId: ocId))
            #expect(row.deleted)
            #expect(row.syncTime > oldSyncTime, "Stamped, so the local derivation finds the deletion by date.")
        }
        let rederived = try #require(manager.pendingWorkingSetChanges(since: Date(timeIntervalSinceNow: -300)))
        #expect(Set(rederived.deleted.map(\.ocId)) == ["a", "b", "f"])
    }

    @Test func aDirectoryDeletedAndRecreatedUnderItsNameKeepsTheNewChildren() async throws {
        let old = makeFolder("d", in: rootItem)
        old.identifier = "d-old"
        let oldChild = makeFile("c", in: old)
        oldChild.identifier = "c-old"
        seed(rootItem, visited: true)
        seed(old, visited: true)
        seed(oldChild, downloaded: true)
        let warmAnchor = try await warmUp()

        rootItem.removeChild(old)
        let recreated = makeFolder("d", in: rootItem)
        recreated.identifier = "d-new"
        let newChild = makeFile("c", in: recreated)
        newChild.identifier = "c-new"
        rootItem.bumpVersion()
        let afterRecreate = try await pass(from: warmAnchor)

        #expect(afterRecreate.deleted.contains("d-old"))
        #expect(!afterRecreate.deleted.contains("c-new"))
        #expect(!afterRecreate.deleted.contains("d-new"))
        #expect(afterRecreate.reported.isSuperset(of: ["d-new"]))
        #expect(manager.itemMetadata(ocId: "d-new")?.deleted == false)
    }

    @Test func aDirectoryMovedToAnotherVisitedParentKeepsItsMaterialisedChildren() async throws {
        let p = makeFolder("p", in: rootItem)
        let q = makeFolder("q", in: rootItem)
        let d = makeFolder("d", in: p)
        let f = makeFile("f", in: d)
        seed(rootItem, visited: true)
        seed(p, visited: true)
        seed(q, visited: true)
        seed(d, visited: true)
        seed(f, downloaded: true)
        let warmAnchor = try await warmUp()

        d.move(to: q)
        let afterMove = try await pass(from: warmAnchor)

        #expect(afterMove.deleted.isEmpty, "A directory found at its new location was moved, not deleted.")
        let movedFile = try #require(manager.itemMetadata(ocId: "f"))
        #expect(movedFile.serverUrl == account.davFilesUrl + "/q/d")
        #expect(movedFile.downloaded)
        #expect(movedFile.deleted == false)
        #expect(try manager.childItems(directoryMetadata: #require(manager.itemMetadata(ocId: "p"))).isEmpty)
    }

    @Test func aRenamedDirectoryIsNotListedAgain() async throws {
        let p = makeFolder("p", in: rootItem)
        let d = makeFolder("d", in: p)
        let f = makeFile("f", in: d)
        seed(rootItem, visited: true)
        seed(p, visited: true)
        seed(d, visited: true)
        seed(f, downloaded: true)
        let warmAnchor = try await warmUp()

        d.move(to: p, renamingTo: "e")
        let afterRename = try await pass(from: warmAnchor)

        #expect(afterRename.paths == ["", "/p"], "The renamed directory keeps its etag, so nothing below it is read again.")
        #expect(afterRename.reported.contains("d"))
        #expect(manager.itemMetadata(ocId: "f")?.serverUrl == account.davFilesUrl + "/p/e")
        #expect(manager.scannedRemoteStateForTesting(ocId: "d") != nil)
    }

    // MARK: - Failures

    @Test func aFailedListingForgetsTheChainAndIsListedAgainNextPass() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let c = makeFolder("c", in: b)
        let f = makeFile("f", in: c)
        let sibling = makeFolder("sibling", in: rootItem)
        seedTree()
        let warmAnchor = try await warmUp()
        try await waitForTheSecondAfter(warmAnchor)

        f.changeVersion(to: "f-v2")
        remoteInterface.enumerateErrorBySuffix = ["/c": NKError(statusCode: 502, fallbackDescription: "Bad Gateway")]
        let failing = try await pass(from: warmAnchor)

        #expect(failing.observer.error == nil)
        #expect(failing.paths == ["", "/a", "/a/b", "/a/b/c"])
        #expect(!failing.reported.contains("f"))
        for ocId in ["a", "b", "c"] {
            #expect(manager.scannedRemoteStateForTesting(ocId: ocId) == nil, "\(ocId) is on the failed chain and is forgotten.")
        }
        #expect(manager.scannedRemoteStateForTesting(ocId: "sibling") != nil, "Siblings keep their state.")
        #expect(manager.scannedRemoteStateForTesting(ocId: NSFileProviderItemIdentifier.rootContainer.rawValue) == nil)
        let advanced = try #require(failing.anchor)
        #expect(advanced.rawValue != warmAnchor.rawValue, "A failed listing does not hold the anchor back.")

        remoteInterface.enumerateErrorBySuffix = [:]
        let retried = try await pass(from: advanced)

        #expect(retried.paths == ["", "/a", "/a/b", "/a/b/c"], "The chain is walked again although nothing changed meanwhile.")
        #expect(retried.reported.contains("f"))
        #expect(manager.scannedRemoteStateForTesting(ocId: "c") != nil)
    }

    @Test func aRootListingFailureListsNothingElseAndAdvancesTheAnchor() async throws {
        let a = makeFolder("a", in: rootItem)
        seed(rootItem, visited: true)
        seed(a, visited: true)
        remoteInterface.enumerateErrorBySuffix = ["/" + account.id: NKError(statusCode: 502, fallbackDescription: "Bad Gateway")]

        let failing = try await pass()

        #expect(failing.observer.error == nil)
        #expect(failing.paths == [""])
        #expect(failing.reported.isEmpty)
        #expect(failing.anchor?.rawValue != anchor.rawValue)
    }

    @Test func anUnreadableStoreHoldsTheAnchorWithoutRequests() async throws {
        let a = makeFolder("a", in: rootItem)
        seed(rootItem, visited: true)
        seed(a, visited: true)
        try manager.breakTableForTesting(ItemMetadataRecord.databaseTableName)

        let pass = try await pass()

        #expect(pass.paths.isEmpty)
        #expect(pass.anchor?.rawValue == anchor.rawValue, "Nothing could be checked, so nothing is claimed as synchronised.")
    }

    // MARK: - Policies

    @Test func anExternalStorageDirectoryIsListedOnEveryPass() async throws {
        let mount = makeFolder("mount", in: rootItem)
        let inside = makeFolder("inside", in: mount)
        let other = makeFolder("other", in: rootItem)
        mount.mountType = FilesDatabaseManager.externalMountType
        inside.mountType = FilesDatabaseManager.externalMountType
        seed(rootItem, visited: true)
        seed(mount, visited: true)
        seed(inside, visited: true)
        seed(other, visited: true)
        let warmAnchor = try await warmUp()

        let idle = try await pass(from: warmAnchor)

        #expect(idle.paths == ["", "/mount", "/mount/inside"], "The server notices changes made behind its back only when the directory is read.")
    }

    @Test func anExternalStorageBelowAnUnchangedDirectoryIsListedOnEveryPass() async throws {
        let parent = makeFolder("parent", in: rootItem)
        let mount = makeFolder("mount", in: parent)
        let inside = makeFolder("inside", in: mount)
        let other = makeFolder("other", in: rootItem)
        mount.mountType = FilesDatabaseManager.externalMountType
        inside.mountType = FilesDatabaseManager.externalMountType
        seed(rootItem, visited: true)
        seed(parent, visited: true)
        seed(mount, visited: true)
        seed(inside, visited: true)
        seed(other, visited: true)
        let warmAnchor = try await warmUp()

        let idle = try await pass(from: warmAnchor)

        #expect(idle.paths == ["", "/parent", "/parent/mount", "/parent/mount/inside"], "The walk has to pass through the unchanged parent to reach the mount.")
    }

    @Test func aMountPointIsReconciledByTheStateItsParentLists() async throws {
        let mount = makeFolder("mount", in: rootItem)
        mount.permissions = "SRGDNVCK"
        mount.permissionsAsListedByParent = "SRGNV"
        let f = makeFile("f", in: mount)
        seed(rootItem, visited: true)
        seed(mount, visited: true)
        seed(f, downloaded: true)
        let warmAnchor = try await warmUp()

        let idle = try await pass(from: warmAnchor)

        #expect(idle.paths == [""], "The server rewrites a mount point's permissions in its parent's listing; the walk compares against that view.")
        #expect(manager.scannedRemoteStateForTesting(ocId: "mount") == "mount-v1|SRGNV")
    }

    @Test func aLockChangeIsReportedWhenTheServerPropagatesIt() async throws {
        let folder = makeFolder("folder", in: rootItem)
        let f = makeFile("f", in: folder)
        seed(rootItem, visited: true)
        seed(folder, visited: true)
        seed(f, downloaded: true)
        let warmAnchor = try await warmUp()

        f.locked = true
        f.lockOwner = "someone"
        let silent = try await pass(from: warmAnchor)
        #expect(!silent.reported.contains("f"), "Without an etag change nothing below the folder is read.")

        f.changeVersion(to: "f-locked")
        let propagated = try await pass(from: warmAnchor)
        #expect(propagated.reported.contains("f"))
        #expect(manager.itemMetadata(ocId: "f")?.lock == true)
    }

    @Test func theRootRowIsNeverReported() async throws {
        let folder = makeFolder("folder", in: rootItem)
        let f = makeFile("f", in: folder)
        seed(rootItem, visited: true)
        seed(folder, visited: true)
        seed(f, downloaded: true)
        let warmAnchor = try await warmUp()

        f.changeVersion(to: "f-v2")
        let changed = try await pass(from: warmAnchor)

        #expect(!changed.reported.contains(NSFileProviderItemIdentifier.rootContainer.rawValue))
        #expect(changed.reported.isSuperset(of: ["folder", "f"]))
    }

    @Test func aScanListingDoesNotMarkADirectoryVisited() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let f = makeFile("f", in: b)
        seed(rootItem, visited: true)
        seed(a)
        seed(b)
        seed(f, downloaded: true)

        _ = try await pass()

        #expect(manager.itemMetadata(ocId: "a")?.visitedDirectory == false, "Only the system's own listings count as visits.")
        #expect(manager.itemMetadata(ocId: "b")?.visitedDirectory == false)
    }
}
