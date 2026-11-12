//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import TestInterface

///
/// A browsed account: a server tree three folder levels deep, and a database which knows it the way the extension would after the user browsed it.
///
/// Every folder was listed, so it is a visited directory. The first file of one bottom folder in ten was downloaded, and so was one file in each of a few folders that were never listed. About a fifth of the rows are materialised, so a working-set scan has real decisions to make.
///
/// The shape and its names are deterministic, so every run and every revision works on the same data.
///
final class BrowsedTreeFixture {
    /// Folders per level; the tree has `width` + `width`² + `width`³ listed folders.
    static let width = 3

    /// Downloaded files in folders that were never listed.
    static let orphans = 5

    static let filesPerFolder = 5
    static let downloadedFolderStride = 10

    let account: Account
    let remoteInterface: MockRemoteInterface
    let database: TemporaryDatabase
    let recorder = EnumeratePathRecorder()
    let writes: DatabaseWriteCounter

    /// The file the changed-file scenario modifies: in a bottom folder which holds no download, so only listing that folder finds the change.
    let changedFile: MockRemoteItem

    /// Where the next working-set pass starts; advanced by every ``runWorkingSetPass()``.
    private(set) var anchor = NSFileProviderSyncAnchor(Data())

    private var changeCount = 0

    ///
    /// - Parameter scenario: Names the account, so concurrently running suites never share the process-wide capabilities cache entry.
    ///
    init(scenario: String) async throws {
        let account = Account(user: "bench-\(scenario)", id: "bench-\(scenario)", serverUrl: "https://bench.nc.com", password: "abcd")
        let root = MockRemoteItem.rootItem(account: account)
        let database = try TemporaryDatabase(account: account, defaults: Benchmark.defaults)
        var visited: [MockRemoteItem] = [root]
        var downloaded: [MockRemoteItem] = []
        var changedFile: MockRemoteItem?
        let width = Self.width
        let middle = width / 2

        func add(_ name: String, to parent: MockRemoteItem, directory: Bool) -> MockRemoteItem {
            Self.makeItem(name: name, parent: parent, directory: directory, account: account)
        }

        for first in 0 ..< width {
            let level1 = add("L1-\(first)", to: root, directory: true)
            visited.append(level1)

            for second in 0 ..< width {
                let level2 = add("L2-\(first)-\(second)", to: level1, directory: true)
                visited.append(level2)

                for third in 0 ..< width {
                    let folder = add("L3-\(first)-\(second)-\(third)", to: level2, directory: true)
                    visited.append(folder)
                    let folderIndex = (first * width + second) * width + third

                    for index in 0 ..< Self.filesPerFolder {
                        let file = add("file-\(first)-\(second)-\(third)-\(index).txt", to: folder, directory: false)

                        if index == 0, folderIndex.isMultiple(of: Self.downloadedFolderStride) {
                            downloaded.append(file)
                        }

                        if first == middle, second == middle, third == middle, index == 1 {
                            changedFile = file
                        }
                    }
                }
            }
        }

        for index in 0 ..< Self.orphans {
            let folder = add("Unlisted-\(index)", to: root, directory: true)
            downloaded.append(add("orphan-\(index).bin", to: folder, directory: false))
        }

        // Seed through the production listing write, as the extension stores what it lists.
        for folder in visited {
            _ = database.manager.depth1ReadUpdateItemMetadatas(
                account: account.ncKitAccount,
                serverUrl: folder.remotePath,
                updatedMetadatas: ([folder] + folder.children).map { $0.toItemMetadata(account: account) },
                keepExistingDownloadState: true
            )
        }

        for file in downloaded {
            var metadata = file.toItemMetadata(account: account)
            metadata.downloaded = true
            database.manager.addItemMetadata(metadata)
        }

        self.account = account
        remoteInterface = MockRemoteInterface(account: account, rootItem: root)
        self.database = database
        writes = DatabaseWriteCounter(database.manager)
        self.changedFile = changedFile!
        recorder.record(on: remoteInterface)

        // A long-lived extension has its capabilities cached; without this the mock parses its JSON on every listing.
        await RetrievedCapabilitiesActor.shared.setCapabilities(forAccount: account.ncKitAccount, capabilities: remoteInterface.directMockCapabilities()!)

        try await Benchmark.waitForNextSecond()
        anchor = try makeEnumerator().syncAnchor(at: Date())
    }

    /// The running totals to subtract a later reading from.
    var counts: BenchmarkCounts {
        BenchmarkCounts(recorder: recorder, writes: writes)
    }

    ///
    /// Change ``changedFile`` on the server, which also changes the etags of every folder above it.
    ///
    func changeFileOnServer() {
        changeCount += 1
        changedFile.changeVersion(to: "v\(changeCount + 1)")
    }

    ///
    /// Create a folder named "New" holding ``filesPerFolder`` files on the server, in the folder of ``changedFile``, which holds no download.
    ///
    /// - Parameter pinned: Whether to set the keep-downloaded flag on that folder's own row first, which the new folder inherits. Unlike "Always keep downloaded" in Finder, this leaves the existing files unpinned and not downloaded.
    ///
    func addFolderOnServer(pinningItsParent pinned: Bool) throws {
        let parent = changedFile.parent!

        if pinned {
            _ = try database.manager.set(keepDownloaded: true, for: parent.toItemMetadata(account: account))
        }

        let folder = Self.makeItem(name: "New", parent: parent, directory: true, account: account)

        for index in 0 ..< Self.filesPerFolder {
            _ = Self.makeItem(name: "new-\(index).txt", parent: folder, directory: false, account: account)
        }

        parent.bumpVersion()
    }

    ///
    /// One working-set change pass from ``anchor``, draining every batch as the system does.
    ///
    /// - Returns: The observer, with the items the pass reported.
    ///
    @discardableResult
    func runWorkingSetPass() async throws -> MockChangeObserver {
        let observer = try MockChangeObserver(enumerator: makeEnumerator())
        try await observer.enumerateChanges(from: anchor)
        anchor = observer.finishes.last!.anchor
        return observer
    }

    private func makeEnumerator() throws -> Enumerator {
        try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: database.manager,
            log: FileProviderLogMock()
        )
    }

    ///
    /// Add an item below `parent`, named and identified by `name`, with one byte of content for a file.
    ///
    static func makeItem(name: String, parent: MockRemoteItem, directory: Bool, account: Account) -> MockRemoteItem {
        let item = MockRemoteItem(
            identifier: name,
            versionIdentifier: "v1",
            name: name,
            remotePath: parent.remotePath + "/" + name,
            directory: directory,
            data: directory ? nil : Data([1]),
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        item.parent = parent
        parent.children.append(item)
        return item
    }
}
