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
