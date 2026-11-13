//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing
import TestInterface

///
/// What a paginated folder enumeration (Nextcloud 31 and later) does to stored children the server no longer returns.
///
/// Every test has its own database and mock server with pagination on and three files per page.
///
@Suite("Paginated enumeration")
struct PaginatedEnumerationTests {
    let account = DatabaseTestSuites.account
    let manager = DatabaseTestSuites.makeManager()
    let rootItem: MockRemoteItem
    let remoteInterface: MockRemoteInterface
    let folder: MockRemoteItem
    let old = Date(timeIntervalSinceNow: -600)
    static let pageSize = 3

    init() {
        rootItem = MockRemoteItem.rootItem(account: account)
        remoteInterface = MockRemoteInterface(account: account, rootItem: rootItem, pagination: true)
        remoteInterface.capabilities = remoteInterface.capabilities.replacingOccurrences(of: ##""major": 28"##, with: ##""major": 31"##)
        folder = MockRemoteItem(
            identifier: "folder", versionIdentifier: "folder-v1", name: "folder", remotePath: rootItem.remotePath + "/folder", directory: true,
            account: account.ncKitAccount, username: account.username, userId: account.id, serverUrl: account.serverUrl
        )
        rootItem.addChild(folder)
        for index in 0 ..< 7 {
            let file = MockRemoteItem(
                identifier: "f\(index)", name: "f\(index).txt", remotePath: folder.remotePath + "/f\(index).txt", data: Data([UInt8(index)]),
                account: account.ncKitAccount, username: account.username, userId: account.id, serverUrl: account.serverUrl
            )
            folder.addChild(file)
        }
    }

    /// Store a row as the server once confirmed it, with a `syncTime` before any listing of this test.
    private func seed(_ metadata: SendableItemMetadata) {
        var row = metadata
        row.uploaded = true
        row.syncTime = old
        manager.addItemMetadata(row)
    }

    private func seedStaleChildren() -> (gone: SendableItemMetadata, goneDirectory: SendableItemMetadata, deep: SendableItemMetadata) {
        var folderRow = folder.toItemMetadata(account: account)
        folderRow.visitedDirectory = true
        seed(folderRow)
        var gone = DatabaseTestSuites.makeFile(ocId: "gone", fileName: "gone.txt", serverUrl: folder.remotePath)
        gone.downloaded = true
        seed(gone)
        var goneDirectory = DatabaseTestSuites.makeFile(ocId: "goneDir", fileName: "GoneDir", serverUrl: folder.remotePath)
        goneDirectory.directory = true
        goneDirectory.visitedDirectory = true
        seed(goneDirectory)
        var deep = DatabaseTestSuites.makeFile(ocId: "deep", fileName: "deep.txt", serverUrl: folder.remotePath + "/GoneDir")
        deep.downloaded = true
        seed(deep)
        return (gone, goneDirectory, deep)
    }

    private func makeEnumerator() throws -> Enumerator {
        try Enumerator(
            enumeratedItemIdentifier: NSFileProviderItemIdentifier(folder.identifier), account: account, remoteInterface: remoteInterface,
            dbManager: manager, pageSize: Self.pageSize, log: FileProviderLogMock()
        )
    }

    @Test func theLastPageTombstonesTheChildrenNoPageReturned() async throws {
        _ = seedStaleChildren()
        let observer = try MockEnumerationObserver(enumerator: makeEnumerator())

        try await observer.enumerateItems()

        #expect(observer.items.count == 7)
        #expect(observer.observedPages.count == 3, "Seven files at three per page.")
        for ocId in ["gone", "goneDir", "deep"] {
            let row = try #require(manager.itemMetadata(ocId: ocId))
            #expect(row.deleted, "\(ocId) was in no page.")
            #expect(row.syncTime > old)
        }
        for index in 0 ..< 7 {
            #expect(manager.itemMetadata(ocId: "f\(index)")?.deleted == false)
        }
        let pending = try #require(manager.pendingWorkingSetChanges(since: old))
        #expect(Set(pending.deleted.map(\.ocId)).isSuperset(of: ["gone", "goneDir", "deep"]), "The working set learns of the deletions by date.")
    }

    @Test func nothingIsTombstonedBeforeTheLastPage() async throws {
        _ = seedStaleChildren()
        let observer = try MockEnumerationObserver(enumerator: makeEnumerator())

        try await observer.enumerateItemsPage(page: NSFileProviderPage.initialPageSortedByName as NSFileProviderPage)

        #expect(observer.items.count < 7)
        #expect(observer.page != nil, "More pages follow.")
        for ocId in ["gone", "goneDir", "deep"] {
            #expect(manager.itemMetadata(ocId: ocId)?.deleted == false, "\(ocId) may still be on a later page.")
        }
    }

    @Test func aContinuationWithoutAListingStartTombstonesNothing() async throws {
        _ = seedStaleChildren()
        let observer = try MockEnumerationObserver(enumerator: makeEnumerator())
        try await observer.enumerateItemsPage(page: NSFileProviderPage.initialPageSortedByName as NSFileProviderPage)
        let handedPage = try #require(observer.page)
        var token = try #require(try JSONSerialization.jsonObject(with: handedPage.rawValue) as? [String: Any])
        token.removeValue(forKey: "startedAt")
        let continuation = try NSFileProviderPage(JSONSerialization.data(withJSONObject: token))

        // A build which did not record the start handed over this token; the rows of its first page cannot be told apart from stale ones.
        try await observer.enumerateItemsPage(page: continuation)
        while let page = observer.page, !observer.isComplete {
            try await observer.enumerateItemsPage(page: page)
        }

        #expect(observer.isComplete)
        for ocId in ["gone", "goneDir", "deep"] {
            #expect(manager.itemMetadata(ocId: ocId)?.deleted == false)
        }
    }

    @Test func aDownloadFinishingDuringTheListingDoesNotLoseTheRow() async throws {
        _ = seedStaleChildren()
        // The third file sits on the second of three pages; its row predates the listing like the stale ones.
        let before = folder.children[3].toItemMetadata(account: account)
        seed(before)
        let observer = try MockEnumerationObserver(enumerator: makeEnumerator())
        try await observer.enumerateItemsPage(page: NSFileProviderPage.initialPageSortedByName as NSFileProviderPage)
        try await observer.enumerateItemsPage(page: #require(observer.page))

        // A download which started before the page rewrote the row stores the row it fetched, downloaded now.
        var finished = before
        finished.downloaded = true
        finished.uploaded = true
        manager.addItemMetadata(finished)
        while let page = observer.page, !observer.isComplete {
            try await observer.enumerateItemsPage(page: page)
        }

        let row = try #require(manager.itemMetadata(ocId: "f3"))
        #expect(!row.deleted, "The row's stamp never goes backwards, so the drop still sees the page's write.")
        #expect(row.downloaded)
        #expect(manager.itemMetadata(ocId: "gone")?.deleted == true)
    }

    @Test func anItemMovedIntoTheFolderDuringTheListingIsKept() async throws {
        _ = seedStaleChildren()
        var elsewhere = DatabaseTestSuites.makeFile(ocId: "moved", fileName: "moved.txt", serverUrl: rootItem.remotePath + "/elsewhere")
        elsewhere.downloaded = true
        seed(elsewhere)
        let observer = try MockEnumerationObserver(enumerator: makeEnumerator())
        try await observer.enumerateItemsPage(page: NSFileProviderPage.initialPageSortedByName as NSFileProviderPage)

        manager.renameItemMetadata(ocId: "moved", newServerUrl: folder.remotePath, newFileName: "moved.txt")
        while let page = observer.page, !observer.isComplete {
            try await observer.enumerateItemsPage(page: page)
        }

        let row = try #require(manager.itemMetadata(ocId: "moved"))
        #expect(!row.deleted, "A move stamps the row, so a listing started before it does not take it for a stale one.")
        #expect(row.serverUrl == folder.remotePath)
    }

    @Test func aContinuationWithoutAListingStartReconcilesNothingEither() async throws {
        var folderRow = folder.toItemMetadata(account: account)
        folderRow.visitedDirectory = true
        seed(folderRow)
        var gone = DatabaseTestSuites.makeFile(ocId: "gone", fileName: "gone.txt", serverUrl: folder.remotePath)
        gone.downloaded = true
        seed(gone)
        let rootEnumerator = try Enumerator(
            enumeratedItemIdentifier: .rootContainer, account: account, remoteInterface: remoteInterface, dbManager: manager, log: FileProviderLogMock()
        )
        try await MockEnumerationObserver(enumerator: rootEnumerator).enumerateItems()
        let observer = try MockEnumerationObserver(enumerator: makeEnumerator())
        try await observer.enumerateItemsPage(page: NSFileProviderPage.initialPageSortedByName as NSFileProviderPage)
        let handedPage = try #require(observer.page)
        var token = try #require(try JSONSerialization.jsonObject(with: handedPage.rawValue) as? [String: Any])
        token.removeValue(forKey: "startedAt")

        try await observer.enumerateItemsPage(page: NSFileProviderPage(JSONSerialization.data(withJSONObject: token)))
        while let page = observer.page, !observer.isComplete {
            try await observer.enumerateItemsPage(page: page)
        }

        #expect(manager.itemMetadata(ocId: "gone")?.deleted == false)
        #expect(manager.scannedRemoteStateForTesting(ocId: folder.identifier) == nil, "Nothing compared the stale file with the server; the scan has to.")
    }

    @Test func aChildWhichArrivesOnALaterPageIsKept() async throws {
        _ = seedStaleChildren()
        // The seventh file sits alone on the last page; its row predates the listing like the stale ones.
        var lastFile = folder.children[6].toItemMetadata(account: account)
        lastFile.downloaded = true
        seed(lastFile)
        let observer = try MockEnumerationObserver(enumerator: makeEnumerator())

        try await observer.enumerateItems()

        let row = try #require(manager.itemMetadata(ocId: "f6"))
        #expect(!row.deleted)
        #expect(row.downloaded, "The page rewrote the row and kept its local state.")
        #expect(manager.itemMetadata(ocId: "gone")?.deleted == true)
    }
}
