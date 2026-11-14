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
/// What an item enumeration the system asked for leaves for the working-set scan to list.
///
/// Every test has its own database and mock server. Directories are enumerated the way Finder walks them, top down, each through its own enumerator.
///
@Suite("Enumerated directory reconciliation")
struct EnumeratedDirectoryReconciliationTests {
    let account = DatabaseTestSuites.isolatedAccount()
    let manager: FilesDatabaseManager
    let rootItem: MockRemoteItem
    let remoteInterface: MockRemoteInterface
    let recorder = EnumeratePathRecorder()

    ///
    /// Seed the capabilities cache of this suite's own account with what `interface` answers.
    ///
    /// Capabilities are cached per account for the whole process, so a suite which fetched them through an
    /// interface of its own would otherwise decide for every other suite on that account which server they
    /// are talking to, depending on which one ran first.
    ///
    private func usePaginatedServer(_ interface: MockRemoteInterface) async {
        guard let capabilities = interface.directMockCapabilities() else {
            return
        }

        await RetrievedCapabilitiesActor.shared.setCapabilities(forAccount: account.ncKitAccount, capabilities: capabilities)
    }

    init() {
        manager = DatabaseTestSuites.makeManager(account: account)
        rootItem = MockRemoteItem.rootItem(account: account)
        remoteInterface = MockRemoteInterface(account: account, rootItem: rootItem)
        recorder.record(on: remoteInterface)
    }

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

    /// Enumerate a container the way the system does: through its own enumerator, every page.
    private func enumerate(_ item: MockRemoteItem) async throws {
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: NSFileProviderItemIdentifier(item.identifier), account: account, remoteInterface: remoteInterface,
            dbManager: manager, log: FileProviderLogMock()
        )
        try await MockEnumerationObserver(enumerator: enumerator).enumerateItems()
    }

    private struct Pass {
        let paths: [String]
        let reported: Set<String>
        let anchor: NSFileProviderSyncAnchor
    }

    /// One working-set change pass from the given anchor, or from an old one: the directories it listed, the items it reported, and the anchor to continue from. Passes chain on their anchors as the system does, so none is mistaken for a continuation of the previous one.
    private func pass(from anchor: NSFileProviderSyncAnchor? = nil) async throws -> Pass {
        recorder.reset()
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet, account: account, remoteInterface: remoteInterface, dbManager: manager, log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        try await observer.enumerateChanges(from: anchor ?? Enumerator.syncAnchor(at: Date(timeIntervalSinceNow: -300), storeIdentity: manager.storeIdentity))
        #expect(observer.error == nil)
        return try Pass(
            paths: recorder.paths.map { $0.replacingOccurrences(of: account.davFilesUrl, with: "") },
            reported: Set(observer.changedItems.map(\.itemIdentifier.rawValue)),
            anchor: #require(observer.finishes.last?.anchor)
        )
    }

    private func stamp(_ item: MockRemoteItem) -> String? {
        manager.scannedRemoteStateForTesting(ocId: item.identifier)
    }

    @Test func aTreeWalkLeavesNothingForTheScanToList() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let c = makeFolder("c", in: b)
        makeFile("f", in: c)

        for directory in [rootItem, a, b, c] {
            try await enumerate(directory)
        }

        #expect(stamp(a) == "a-v1|RGDNVW")
        #expect(stamp(b) == "b-v1|RGDNVW")
        #expect(stamp(c) == "c-v1|RGDNVW")
        let idle = try await pass()
        #expect(idle.paths == [""], "Every directory the system visited is reconciled; the pass costs the root listing.")
    }

    @Test func aChangeBelowAReconciledDirectoryIsStillFound() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let c = makeFolder("c", in: b)
        let f = makeFile("f", in: c)
        for directory in [rootItem, a, b, c] {
            try await enumerate(directory)
        }

        f.changeVersion(to: "f-v2")
        let changed = try await pass()

        #expect(changed.paths == ["", "/a", "/a/b", "/a/b/c"], "The server bumps every ancestor's etag, so the walk follows the path.")
        #expect(changed.reported.contains("f"))
    }

    @Test func theAncestorsOfAPendingItemAreLeftToTheScan() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let c = makeFolder("c", in: b)
        let g = makeFile("g", in: c)
        try await enumerate(rootItem)
        // Downloaded before the walk, so every ancestor has something pending below until c itself is listed.
        var downloaded = g.toItemMetadata(account: account)
        downloaded.downloaded = true
        manager.addItemMetadata(downloaded)

        try await enumerate(a)
        try await enumerate(b)
        try await enumerate(c)

        #expect(stamp(c) == "c-v1|RGDNVW")
        #expect(stamp(b) == nil, "Only the directory listed is recorded; an ancestor's stored etag may be refreshed without its children being compared.")
        #expect(stamp(a) == nil)
        let first = try await pass()
        #expect(first.paths == ["", "/a", "/a/b"], "The scan reconciles the ancestors and skips c.")
        let second = try await pass(from: first.anchor)
        #expect(second.paths == [""])
    }

    @Test func aChangeBelowADirectoryWhoseRowAnotherListingRefreshedIsStillFound() async throws {
        let a = makeFolder("a", in: rootItem)
        let b = makeFolder("b", in: a)
        let f = makeFile("f", in: b)
        let d = makeFolder("d", in: b)
        for directory in [rootItem, a, b] {
            try await enumerate(directory)
        }
        var downloaded = f.toItemMetadata(account: account)
        downloaded.downloaded = true
        manager.addItemMetadata(downloaded)
        #expect(stamp(b) == "b-v1|RGDNVW")

        // f changes on the server; a's listing refreshes b's stored row without listing b, and d's listing must not seal b at that newer etag.
        f.changeVersion(to: "f-v2")
        try await enumerate(a)
        try await enumerate(d)

        #expect(stamp(b) == "b-v1|RGDNVW", "b keeps the state at which its own listing compared its children.")
        let changed = try await pass()
        #expect(changed.paths == ["", "/a", "/a/b"])
        #expect(changed.reported.contains("f"))
    }

    @Test func aDirectoryWhoseStoredStateMovedOnWhileItsPagesWereReadIsLeftToTheScan() async throws {
        let paginating = MockRemoteInterface(account: account, rootItem: rootItem, pagination: true)
        paginating.capabilities = paginating.capabilities.replacingOccurrences(of: ##""major": 28"##, with: ##""major": 31"##)
        let p = makeFolder("p", in: rootItem)
        let x = makeFolder("x", in: p)
        for index in 0 ..< 7 {
            makeFile("f\(index)", in: x)
        }
        // The mock serves one paginated listing at a time, so only x is read through the paginating interface; both read the same tree.
        try await enumerate(rootItem)
        try await enumerate(p)
        await usePaginatedServer(paginating)
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: NSFileProviderItemIdentifier(x.identifier), account: account, remoteInterface: paginating, dbManager: manager,
            pageSize: 3, log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        try await observer.enumerateItemsPage(page: NSFileProviderPage.initialPageSortedByName as NSFileProviderPage)

        // Something changes under x while its pages are read; p's listing stores x's new etag without listing x.
        x.changeVersion(to: "x-v2")
        try await enumerate(p)
        while let page = observer.page, !observer.isComplete {
            try await observer.enumerateItemsPage(page: page)
        }

        #expect(observer.isComplete)
        #expect(stamp(x) == nil, "The pages came from the state before the change.")
    }

    @Test func aDirectoryWithAChildTheScanHasToListIsLeftToTheScan() async throws {
        let a = makeFolder("a", in: rootItem)
        let unvisited = makeFolder("u", in: a)
        let g = makeFile("g", in: unvisited)
        try await enumerate(rootItem)
        // Downloaded by identifier, without its directory ever having been listed.
        var downloaded = g.toItemMetadata(account: account)
        downloaded.downloaded = true
        manager.addItemMetadata(downloaded)

        try await enumerate(a)

        #expect(stamp(a) == nil, "A child holding a materialised item below is left for the scan to descend into.")
        let first = try await pass()
        #expect(first.paths == ["", "/a", "/a/u"])
        let second = try await pass(from: first.anchor)
        #expect(second.paths == [""], "The scan reconciled the chain.")
    }

    @Test func aMountPointIsLeftToTheScan() async throws {
        let mount = makeFolder("mount", in: rootItem)
        mount.mountType = "group"
        mount.permissions = "SRGDNVCK"
        mount.permissionsAsListedByParent = "SRGNV"
        makeFile("f", in: mount)

        try await enumerate(rootItem)
        try await enumerate(mount)

        #expect(stamp(mount) == nil, "The scan keeps a mount point's state as the parent lists it.")
        let first = try await pass()
        #expect(first.paths == ["", "/mount"])
        #expect(stamp(mount) == "mount-v1|SRGNV")
        let second = try await pass(from: first.anchor)
        #expect(second.paths == [""])
    }

    @Test func aDirectoryAboveAnExternalStorageIsLeftToTheScan() async throws {
        let a = makeFolder("a", in: rootItem)
        let mount = makeFolder("mount", in: a)
        mount.mountType = FilesDatabaseManager.externalMountType
        makeFile("f", in: mount)
        for directory in [rootItem, a, mount] {
            try await enumerate(directory)
        }

        let first = try await pass()
        let second = try await pass(from: first.anchor)
        #expect(first.paths == ["", "/a", "/a/mount"])
        #expect(second.paths == ["", "/a", "/a/mount"], "Whatever state the enumeration recorded for the directory above, the server notices changes on an external storage only when it is read.")
    }

    @Test func aFailedEnumerationReconcilesNothing() async throws {
        let a = makeFolder("a", in: rootItem)
        makeFile("f", in: a)
        try await enumerate(rootItem)
        remoteInterface.enumerateErrorBySuffix = ["/a": NKError(statusCode: 502, fallbackDescription: "Bad Gateway")]

        await #expect(throws: (any Error).self) {
            try await enumerate(a)
        }

        #expect(stamp(a) == nil)
    }

    @Test func theRootContainerIsNeverReconciledByItsEnumeration() async throws {
        makeFolder("a", in: rootItem)

        try await enumerate(rootItem)

        #expect(manager.scannedRemoteStateForTesting(ocId: NSFileProviderItemIdentifier.rootContainer.rawValue) == nil)
        #expect(manager.itemMetadata(ocId: NSFileProviderItemIdentifier.rootContainer.rawValue)?.visitedDirectory == true)
    }

    @Test func aPaginatedEnumerationReconcilesOnItsLastPage() async throws {
        let paginating = MockRemoteInterface(account: account, rootItem: rootItem, pagination: true)
        paginating.capabilities = paginating.capabilities.replacingOccurrences(of: ##""major": 28"##, with: ##""major": 31"##)
        let folder = makeFolder("folder", in: rootItem)
        for index in 0 ..< 7 {
            makeFile("f\(index)", in: folder)
        }
        let rootEnumerator = try Enumerator(
            enumeratedItemIdentifier: .rootContainer, account: account, remoteInterface: paginating, dbManager: manager, log: FileProviderLogMock()
        )
        try await MockEnumerationObserver(enumerator: rootEnumerator).enumerateItems()
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: NSFileProviderItemIdentifier(folder.identifier), account: account, remoteInterface: paginating,
            dbManager: manager, pageSize: 3, log: FileProviderLogMock()
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)

        try await observer.enumerateItemsPage(page: NSFileProviderPage.initialPageSortedByName as NSFileProviderPage)
        #expect(stamp(folder) == nil, "Later pages may still bring a child the scan has to list.")

        while let page = observer.page, !observer.isComplete {
            try await observer.enumerateItemsPage(page: page)
        }

        #expect(observer.isComplete)
        #expect(stamp(folder) == "folder-v1|RGDNVW")
    }
}
