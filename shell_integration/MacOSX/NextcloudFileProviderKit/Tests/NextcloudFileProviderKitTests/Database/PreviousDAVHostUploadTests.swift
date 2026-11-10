//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

@Suite("Uploads across a DAV host change")
struct PreviousDAVHostUploadTests {
    @Test func unfinishedUploadSurvivesCurrentHostListing() throws {
        let identifier = NSFileProviderDomainIdentifier("previous-dav-host-\(UUID().uuidString)")
        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent(identifier.rawValue, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let oldAccount = Account(user: "alice", id: "alice", serverUrl: "https://old.example.com", password: "password")
        let currentAccount = Account(user: "alice", id: "alice", serverUrl: "https://current.example.com", password: "password")

        func openDatabase(_ account: Account) throws -> FilesDatabaseManager {
            try FilesDatabaseManager(
                account: account,
                databaseDirectory: directory,
                fileProviderDomainIdentifier: identifier,
                log: FileProviderLogMock(),
                defaults: DatabaseTestSuites.defaults
            )
        }

        let itemID = "retained-ocid"
        let chunkID = "pending-upload-chunks"
        do {
            let old = try openDatabase(oldAccount)
            var pending = DatabaseTestSuites.makeFile(ocId: itemID, fileName: "notes.txt", account: oldAccount)
            pending.uploaded = false
            pending.status = Status.uploadError.rawValue
            pending.chunkUploadId = chunkID
            old.addItemMetadata(pending)
            #expect(old.itemMetadata(ocId: itemID)?.chunkUploadId == chunkID)
        }

        let current = try openDatabase(currentAccount)
        #expect(current.itemMetadata(ocId: itemID)?.chunkUploadId == chunkID)
        var listing = DatabaseTestSuites.makeFile(ocId: itemID, fileName: "notes.txt", account: currentAccount)
        listing.uploaded = true
        listing.status = Status.normal.rawValue
        _ = current.addItemMetadataPreservingLocalState(listing)
        let stored = try #require(current.allItemMetadatasForTesting().first { $0.ocId == itemID })
        #expect(stored.chunkUploadId == chunkID)
        #expect(stored.status == Status.uploadError.rawValue)
    }
}
