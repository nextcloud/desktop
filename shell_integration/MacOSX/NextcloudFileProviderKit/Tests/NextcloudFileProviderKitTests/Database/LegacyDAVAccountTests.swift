//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import TestInterface
import Testing

@Suite("DAV account changes in the metadata database")
struct LegacyDAVAccountTests {
    enum AccountChange: CaseIterable {
        case serverURL
        case backendUserID
        case unchanged

        func applying(to account: Account) -> Account {
            Account(
                user: account.username,
                id: self == .backendUserID ? "new-alice-id" : account.id,
                serverUrl: self == .serverURL ? "https://current.example.com" : account.serverUrl,
                password: account.password
            )
        }
    }

    private struct PausedDAVResponse {
        let requestStarted: AsyncStream<Void>
        let releaseResponse: AsyncStream<Void>.Continuation

        init(remote: MockRemoteInterface) {
            let (requestStarted, started) = AsyncStream<Void>.makeStream()
            let (released, releaseResponse) = AsyncStream<Void>.makeStream()
            self.requestStarted = requestStarted
            self.releaseResponse = releaseResponse
            remote.enumerateHandler = {
                started.yield(())
                started.finish()
                var iterator = released.makeAsyncIterator()
                _ = await iterator.next()
            }
        }
    }

    private static let previousAccount = Account(
        user: "alice",
        id: "alice-id",
        serverUrl: "https://previous.example.com",
        password: "password"
    )

    private let domain = NSFileProviderDomainIdentifier("dav-account-change-\(UUID().uuidString)")
    private let directory: URL

    init() throws {
        directory = FileManager.default.temporaryDirectory.appendingPathComponent(domain.rawValue, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
    }

    @Test(arguments: [AccountChange.serverURL, .backendUserID])
    func currentAccountCannotReplacePendingUploadWithMismatchingLegacyDetails(change: AccountChange) throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let previous = Self.previousAccount
        let current = change.applying(to: previous)
        var pending = DatabaseTestSuites.makeFile(
            ocId: "pending-file",
            fileName: "Report.txt",
            account: previous
        )
        pending.account = current.ncKitAccount
        pending.downloaded = true
        pending.uploaded = false
        pending.status = Status.uploadError.rawValue
        pending.chunkUploadId = "pending-edit-upload"
        // Leave one account field missing so the imported row retains its absolute parent URL.
        if change == .serverURL {
            pending.userId = ""
        } else {
            pending.urlBase = ""
        }
        _ = try FilesDatabaseManager.writeDatabaseWithoutSavedDAVAccountForTesting(
            at: directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension),
            metadata: [pending]
        )

        let manager = try openDatabase(account: current)
        #expect(manager.itemMetadata(ocId: pending.ocId) == nil)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: pending.remotePath()) == nil)
        #expect(manager.materialisedItemMetadatas()?.isEmpty == true)

        var listed = DatabaseTestSuites.makeFile(ocId: pending.ocId, fileName: pending.fileName, account: current)
        listed.uploaded = true
        #expect(throws: NKError.invalidData) {
            try manager.persistItemMetadataFromPagedListing(listed)
        }
        manager.addItemMetadata(listed)

        let saved = try #require(manager.itemMetadatas(account: current.ncKitAccount).first { $0.ocId == pending.ocId })
        #expect(saved.remotePath() == pending.remotePath())
        #expect(saved.status == Status.uploadError.rawValue)
        #expect(saved.chunkUploadId == pending.chunkUploadId)
    }

    @Test(arguments: AccountChange.allCases)
    func reopeningClearsLockTokenWhenTheDAVAccountChanges(change: AccountChange) throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let previous = Self.previousAccount
        let current = change.applying(to: previous)
        var file = DatabaseTestSuites.makeFile(ocId: "locked-file", fileName: "Report.txt", account: previous)
        file.lockToken = "saved-dav-lock"
        do {
            let manager = try openDatabase(account: previous)
            manager.addItemMetadata(file)
            try #require(manager.itemMetadata(ocId: file.ocId)?.lockToken == file.lockToken)
        }

        let reopened = try openDatabase(account: current)
        let saved = try #require(reopened.itemMetadata(ocId: file.ocId))
        #expect(saved.remotePath() == current.davFilesUrl + "/" + file.fileName)
        #expect(saved.lockToken == (change == .unchanged ? file.lockToken : nil))
    }

    @Test
    func replacingPasswordResetsInterruptedTransfersWithoutChangingStoreIdentity() throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let previous = Self.previousAccount
        let newCredentials = Account(
            user: previous.username,
            id: previous.id,
            serverUrl: previous.serverUrl,
            password: "new-password"
        )
        let manager = try openDatabase(account: previous)
        let transfers: [(before: Status, after: Status)] = [
            (.inDownload, .normal),
            (.downloading, .normal),
            (.uploading, .uploadError),
            (.inUpload, .inUpload)
        ]
        for (before, _) in transfers {
            var item = DatabaseTestSuites.makeFile(
                ocId: "item-\(before.rawValue)",
                fileName: "File \(before.rawValue).txt",
                account: previous
            )
            item.status = before.rawValue
            item.session = before == .uploading ? "upload-attempt" : nil
            manager.addItemMetadata(item)
        }

        let replacement = try manager.replacingDAVAccount(with: newCredentials)

        #expect(replacement.storeIdentity == manager.storeIdentity)
        for (before, after) in transfers {
            let item = try #require(replacement.itemMetadata(ocId: "item-\(before.rawValue)"))
            #expect(item.status == after.rawValue)
            #expect(item.session == (before == .uploading ? "" : nil))
        }
    }

    @Test(arguments: [AccountChange.serverURL, .backendUserID])
    func previousManagerCannotSaveADAVResponseAfterDAVAccountChange(change: AccountChange) async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let previous = Self.previousAccount
        let current = change.applying(to: previous)
        var file = DatabaseTestSuites.makeFile(
            ocId: "materialized-file",
            fileName: "Report.txt",
            account: previous
        )
        file.downloaded = true
        file.keepDownloaded = true
        let previousManager = try openDatabase(account: previous)
        previousManager.addItemMetadata(file)
        try #require(previousManager.itemMetadata(ocId: file.ocId) != nil)

        let currentManager = try openDatabase(account: current)
        var delayedResponse = file
        delayedResponse.downloaded = false
        delayedResponse.keepDownloaded = false
        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            _ = try await previousManager.addItemMetadataFromRemoteRead(
                delayedResponse,
                requestedURL: delayedResponse.remotePath(),
                isCancelled: { false }
            )
        }

        previousManager.addItemMetadata(delayedResponse)
        let retained = try #require(currentManager.itemMetadata(ocId: file.ocId))
        #expect(retained.remotePath() == current.davFilesUrl + "/" + file.fileName)
        #expect(retained.downloaded)
        #expect(retained.keepDownloaded)
    }

    @Test(arguments: [AccountChange.serverURL, .backendUserID])
    func previousWorkingSetEnumeratorCannotReportAnEmptySnapshotAfterDAVAccountChange(change: AccountChange) async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let previous = Self.previousAccount
        let previousManager = try openDatabase(account: previous)
        let (folder, remote) = cacheVisitedFolder(in: previousManager, account: previous)
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            remoteInterface: remote,
            dbManager: previousManager,
            log: FileProviderLogMock()
        )
        let currentManager = try openDatabase(account: change.applying(to: previous))

        let observer = MockEnumerationObserver(enumerator: enumerator)
        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await observer.enumerateItems()
        }
        #expect(observer.items.isEmpty)
        #expect(observer.observedPages.isEmpty)
        #expect(remote.readOperationCount == 0)
        #expect(currentManager.itemMetadata(ocId: folder.ocId)?.visitedDirectory == true)
    }

    @Test(arguments: [AccountChange.serverURL, .backendUserID])
    func previousFolderEnumeratorCannotReportDeletionAfterDAVAccountChange(change: AccountChange) async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let previous = Self.previousAccount
        let previousManager = try openDatabase(account: previous)
        let (folder, remote) = cacheVisitedFolder(in: previousManager, account: previous)
        remote.enumerateErrorBySuffix = ["/" + folder.fileName: NKError(statusCode: 404, fallbackDescription: "Missing folder")]
        let paused = PausedDAVResponse(remote: remote)
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .init(folder.ocId),
            remoteInterface: remote,
            dbManager: previousManager,
            log: FileProviderLogMock()
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        let read = Task {
            do {
                try await observer.enumerateChanges()
                return nil as NSFileProviderError.Code?
            } catch let error as NSFileProviderError {
                return error.code
            } catch {
                Issue.record("Unexpected enumeration error: \(error)")
                return nil
            }
        }
        defer {
            read.cancel()
            paused.releaseResponse.finish()
        }
        try #require(try await nextTestValue(from: paused.requestStarted) != nil)
        let currentManager = try openDatabase(account: change.applying(to: previous))
        paused.releaseResponse.finish()

        #expect(try await testTaskValue(of: read) == .syncAnchorExpired)
        #expect(remote.readOperationCount == 1)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(observer.finishes.isEmpty)
        #expect(currentManager.itemMetadata(ocId: folder.ocId)?.deleted == false)
    }

    @Test(arguments: [AccountChange.serverURL, .backendUserID])
    func previousSingleItemReadCannotReturnMetadataAfterDAVAccountChange(change: AccountChange) async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let previous = Self.previousAccount
        let previousManager = try openDatabase(account: previous)
        let (folder, remote) = cacheVisitedFolder(in: previousManager, account: previous)
        let paused = PausedDAVResponse(remote: remote)
        let read = Task {
            await Enumerator.readServerUrl(
                folder.remotePath(),
                remoteInterface: remote,
                dbManager: previousManager,
                depth: .target,
                log: FileProviderLogMock()
            )
        }
        defer {
            read.cancel()
            paused.releaseResponse.finish()
        }
        try #require(try await nextTestValue(from: paused.requestStarted) != nil)
        let current = change.applying(to: previous)
        let currentManager = try openDatabase(account: current)
        paused.releaseResponse.finish()

        let result = try await testTaskValue(of: read)
        #expect(result.error == .davAccountChanged)
        #expect(result.metadatas == nil)
        #expect(result.changes == nil)
        #expect(remote.readOperationCount == 1)
        #expect(currentManager.itemMetadata(ocId: folder.ocId)?.remotePath() == current.davFilesUrl + "/" + folder.fileName)
    }

    private func cacheVisitedFolder(
        in manager: FilesDatabaseManager,
        account: Account
    ) -> (SendableItemMetadata, MockRemoteInterface) {
        let root = MockRemoteItem.rootItem(account: account)
        let folder = MockRemoteItem(
            identifier: "team-folder",
            name: "Team",
            remotePath: account.davFilesUrl + "/Team",
            directory: true,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        root.children = [folder]
        folder.parent = root
        var metadata = folder.toItemMetadata(account: account)
        metadata.visitedDirectory = true
        manager.addItemMetadata(metadata)
        return (metadata, MockRemoteInterface(account: account, rootItem: root))
    }

    private func openDatabase(account: Account) throws -> FilesDatabaseManager {
        try FilesDatabaseManager(
            account: account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: domain,
            log: FileProviderLogMock(),
            defaults: DatabaseTestSuites.defaults
        )
    }
}
