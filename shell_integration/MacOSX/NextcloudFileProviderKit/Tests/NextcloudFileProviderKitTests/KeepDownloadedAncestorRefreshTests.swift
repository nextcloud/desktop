//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing
import TestInterface

///
/// Changing "Always keep downloaded" refreshes "Remove downloaded items" on every folder above the change (#11238).
///
struct KeepDownloadedAncestorRefreshTests {
    let account = DatabaseTestSuites.account
    let dbManager = DatabaseTestSuites.makeManager()
    let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier("KeepDownloadedAncestorRefreshTests"), displayName: "Test")
    let rootItem: MockRemoteItem
    let projects: MockRemoteItem
    let pinned: MockRemoteItem
    let subfolder: MockRemoteItem
    let firstFile: MockRemoteItem
    let deepFile: MockRemoteItem
    let cousinFile: MockRemoteItem

    /// Projects holds the folder that gets pinned; every file in it is downloaded and nothing else under Projects is.
    init() {
        rootItem = MockRemoteItem.rootItem(account: account)
        projects = Self.makeItem("projects", directory: true, parent: rootItem, account: account)
        pinned = Self.makeItem("pinned", directory: true, parent: projects, account: account)
        subfolder = Self.makeItem("subfolder", directory: true, parent: pinned, account: account)
        firstFile = Self.makeItem("first", directory: false, parent: pinned, account: account)
        deepFile = Self.makeItem("deep", directory: false, parent: subfolder, account: account)
        cousinFile = Self.makeItem("cousin", directory: false, parent: subfolder, account: account)

        for item in [rootItem, projects, pinned, subfolder, firstFile, deepFile, cousinFile] {
            var metadata = item.toItemMetadata(account: account)
            metadata.visitedDirectory = metadata.directory
            metadata.downloaded = !metadata.directory
            dbManager.addItemMetadata(metadata)
        }
    }

    static func makeItem(_ name: String, directory: Bool, parent: MockRemoteItem, account: Account) -> MockRemoteItem {
        let item = MockRemoteItem(
            identifier: name,
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

    func item(_ mockItem: MockRemoteItem) throws -> Item {
        let metadata = try #require(dbManager.itemMetadata(ocId: mockItem.identifier))

        return Item(
            metadata: metadata,
            parentItemIdentifier: NSFileProviderItemIdentifier(mockItem.parent?.identifier ?? NSFileProviderItemIdentifier.rootContainer.rawValue),
            account: account,
            remoteInterface: MockRemoteInterface(account: account, rootItem: rootItem),
            dbManager: dbManager
        )
    }

    /// Sets the flag the way ``Item/set(keepDownloaded:domain:)`` does and returns the files whose ancestors were asked to refresh.
    func setKeepDownloaded(_ keepDownloaded: Bool, on mockItem: MockRemoteItem) async throws -> Set<String> {
        let target = try item(mockItem)
        var refreshed = Set<String>()
        _ = try dbManager.set(keepDownloaded: keepDownloaded, for: target.metadata)

        try await target.propagate(
            keepDownloaded: keepDownloaded,
            domain: domain,
            signal: { _, _, _, _ in },
            refreshAncestorsOfFiles: { refreshed.formUnion($0) }
        )

        return refreshed
    }

    func projectsOffersRemoveDownloadedItems() throws -> Bool {
        try dbManager.hasEvictableDescendantFile(directoryMetadata: #require(dbManager.itemMetadata(ocId: projects.identifier)))
    }

    @Test func pinningAFolderRefreshesTheFoldersAboveItsDownloadedFiles() async throws {
        let refreshed = try await setKeepDownloaded(true, on: pinned)

        #expect(refreshed == [firstFile.identifier, deepFile.identifier, cousinFile.identifier])
        #expect(dbManager.ancestorContainerIdentifiers(ofFileItemsWithOcIds: refreshed).isSuperset(of: [NSFileProviderItemIdentifier(projects.identifier), .rootContainer]))
        #expect(try projectsOffersRemoveDownloadedItems() == false)
    }

    @Test func unpinningAFolderRefreshesTheFoldersAboveItsDownloadedFiles() async throws {
        _ = try await setKeepDownloaded(true, on: pinned)

        let refreshed = try await setKeepDownloaded(false, on: pinned)

        #expect(refreshed == [firstFile.identifier, deepFile.identifier, cousinFile.identifier])
        #expect(try projectsOffersRemoveDownloadedItems())
    }

    @Test func theRefreshSeesEveryFlagTheChangeWrote() async throws {
        var answersWhenRefreshed = [Bool]()

        for keepDownloaded in [true, false] {
            let target = try item(pinned)
            _ = try dbManager.set(keepDownloaded: keepDownloaded, for: target.metadata)
            try await target.propagate(
                keepDownloaded: keepDownloaded,
                domain: domain,
                signal: { _, _, _, _ in },
                refreshAncestorsOfFiles: { _ in answersWhenRefreshed.append((try? projectsOffersRemoveDownloadedItems()) ?? false) }
            )
        }

        #expect(answersWhenRefreshed == [false, true])
    }

    @Test func aFailedSignalStillRefreshesTheFoldersAboveTheFile() async throws {
        struct SignalFailed: Error {}
        let target = try item(firstFile)
        var refreshed = Set<String>()
        _ = try dbManager.set(keepDownloaded: true, for: target.metadata)

        await #expect(throws: SignalFailed.self) {
            try await target.propagate(
                keepDownloaded: true,
                domain: domain,
                signal: { _, _, _, _ in throw SignalFailed() },
                refreshAncestorsOfFiles: { refreshed.formUnion($0) }
            )
        }

        #expect(refreshed == [firstFile.identifier])
    }

    @Test func pinningADownloadedFileRefreshesTheFoldersAboveIt() async throws {
        let refreshed = try await setKeepDownloaded(true, on: firstFile)

        #expect(refreshed == [firstFile.identifier])
    }

    @Test func pinningAFolderWithNothingDownloadedRefreshesNothing() async throws {
        for file in [firstFile, deepFile, cousinFile] {
            var metadata = try #require(dbManager.itemMetadata(ocId: file.identifier))
            metadata.downloaded = false
            dbManager.addItemMetadata(metadata)
        }

        let refreshed = try await setKeepDownloaded(true, on: pinned)

        #expect(refreshed.isEmpty)
    }

    @Test func unpinningOneFileRefreshesTheFoldersAboveTheCousinsItPins() async throws {
        _ = try await setKeepDownloaded(true, on: pinned)
        var cousin = try #require(dbManager.itemMetadata(ocId: cousinFile.identifier))
        cousin.keepDownloaded = false
        dbManager.addItemMetadata(cousin)

        let refreshed = try await setKeepDownloaded(false, on: deepFile)

        #expect(refreshed == [deepFile.identifier, cousinFile.identifier])
    }
}
