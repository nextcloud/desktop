//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct ItemMetadataTests {
    @Test(arguments: [false, true])
    func remoteMergePreservesLocalStateAcrossMoves(storedValue: Bool) {
        var existing = SendableItemMetadata.rawRow(ocId: "existing")
        existing.directory = true
        existing.serverUrl = "https://example.com/old-parent"
        existing.fileName = "old-name.txt"
        existing.downloaded = storedValue
        existing.keepDownloaded = storedValue
        existing.visitedDirectory = storedValue
        existing.lockToken = storedValue ? "stored-lock" : nil
        existing.etag = "old-etag"

        var incoming = SendableItemMetadata.rawRow(ocId: "incoming")
        incoming.directory = true
        incoming.serverUrl = "https://example.com/new-parent"
        incoming.fileName = "new-name.txt"
        incoming.etag = "new-etag"
        incoming.size = 1024
        incoming.downloaded = !storedValue
        incoming.keepDownloaded = !storedValue
        incoming.visitedDirectory = !storedValue
        incoming.lockToken = "incoming-lock"
        incoming.status = Status.uploadError.rawValue
        incoming.chunkUploadId = "incoming-upload"
        incoming.sessionError = "incoming-error"

        let merged = RemoteMetadataMerger.merge(incoming, preservingLocalStateFrom: existing, preserveVisitedDirectory: true)

        #expect(merged.downloaded == storedValue)
        #expect(merged.keepDownloaded == storedValue)
        #expect(merged.visitedDirectory == storedValue)
        #expect(merged.lockToken == existing.lockToken)
        #expect(merged.ocId == incoming.ocId)
        #expect(merged.serverUrl == incoming.serverUrl)
        #expect(merged.fileName == incoming.fileName)
        #expect(merged.etag == incoming.etag)
        #expect(merged.size == incoming.size)
        #expect(merged.status == incoming.status)
        #expect(merged.chunkUploadId == incoming.chunkUploadId)
        #expect(merged.sessionError == incoming.sessionError)
    }

    @Test(arguments: [false, true], [false, true])
    func remoteMergeRespectsVisitedDirectoryPreservation(preserveVisitedDirectory: Bool, storedVisitedDirectory: Bool) {
        var existing = SendableItemMetadata.rawRow(ocId: "item")
        existing.directory = true
        existing.visitedDirectory = storedVisitedDirectory
        var incoming = existing
        incoming.visitedDirectory = !storedVisitedDirectory

        let merged = RemoteMetadataMerger.merge(incoming, preservingLocalStateFrom: existing, preserveVisitedDirectory: preserveVisitedDirectory)

        #expect(merged.visitedDirectory == (preserveVisitedDirectory ? storedVisitedDirectory : incoming.visitedDirectory))
    }

    @Test(arguments: [false, true], [nil, "stored-content-version"] as [String?])
    func remoteMergePreservesContentVersionOnlyForUnchangedEtag(unchangedEtag: Bool, storedContentVersion: String?) {
        var existing = SendableItemMetadata.rawRow(ocId: "item")
        existing.etag = "stored-etag"
        existing.fileProviderContentVersion = storedContentVersion
        var incoming = existing
        incoming.etag = unchangedEtag ? existing.etag : "new-etag"
        incoming.fileProviderContentVersion = "incoming-content-version"

        let merged = RemoteMetadataMerger.merge(incoming, preservingLocalStateFrom: existing, preserveVisitedDirectory: true)

        #expect(merged.fileProviderContentVersion == (unchangedEtag ? storedContentVersion : incoming.fileProviderContentVersion))
        #expect(merged.etag == incoming.etag)
    }

    @Test(arguments: [false, true], [false, true])
    func remoteMergeRespectsDownloadStatePolicy(preserveDownloadState: Bool, incomingDownloaded: Bool) {
        var existing = SendableItemMetadata.rawRow(ocId: "item")
        existing.downloaded = !incomingDownloaded
        var incoming = existing
        incoming.downloaded = incomingDownloaded

        let merged = RemoteMetadataMerger.merge(
            incoming, preservingLocalStateFrom: existing, preserveVisitedDirectory: true,
            preserveDownloadState: preserveDownloadState
        )

        #expect(merged.downloaded == (preserveDownloadState ? existing.downloaded : incomingDownloaded))
    }

    @Test(arguments: [false, true], [nil, "stored-lock"] as [String?])
    func remoteMergeRespectsExplicitLockClearing(clearLockToken: Bool, storedLockToken: String?) {
        var existing = SendableItemMetadata.rawRow(ocId: "item")
        existing.lockToken = storedLockToken
        var incoming = existing
        incoming.lockToken = "incoming-lock"

        let merged = RemoteMetadataMerger.merge(
            incoming, preservingLocalStateFrom: existing, preserveVisitedDirectory: true,
            clearLockToken: clearLockToken
        )

        #expect(merged.lockToken == (clearLockToken ? nil : storedLockToken))
    }

    @Test func thumbnailUrlCorrect() {
        let account =
            Account(user: "user", id: "id", serverUrl: "https://examplecloud.com", password: "bla")
        var item = SendableItemMetadata(ocId: "ec-test", fileName: "test.txt", account: account)
        item.fileId = "test"
        item.hasPreview = true
        let expectedUrl = URL(string: "https://examplecloud.com/index.php/core/preview?fileId=test&x=250.0&y=250.0&a=true")
        #expect(expectedUrl != nil)
        #expect(item.thumbnailUrl(size: .init(width: 250, height: 250)) == expectedUrl)
    }

    @Test func canonicalEquivalentLocationsCompareEqual() {
        let account = Account(user: "user", id: "id", serverUrl: "https://examplecloud.com", password: "bla")
        var nfcItem = SendableItemMetadata(ocId: "nfc", fileName: "pr\u{00EA}t.pdf", account: account)
        var nfdItem = SendableItemMetadata(ocId: "nfd", fileName: "pre\u{0302}t.pdf", account: account)

        nfcItem.serverUrl = account.davFilesUrl
        nfdItem.serverUrl = account.davFilesUrl

        #expect(nfcItem.hasSameLocation(as: nfdItem))
    }

    @Test func differentLocationsDoNotCompareEqual() {
        let account = Account(user: "user", id: "id", serverUrl: "https://examplecloud.com", password: "bla")
        let first = SendableItemMetadata(ocId: "first", fileName: "one.txt", account: account)
        let second = SendableItemMetadata(ocId: "second", fileName: "two.txt", account: account)

        #expect(!first.hasSameLocation(as: second))
    }

    @Test func canonicalEquivalentServerUrlsCompareEqual() {
        let account = Account(user: "user", id: "id", serverUrl: "https://examplecloud.com", password: "bla")
        var nfcItem = SendableItemMetadata(ocId: "nfc", fileName: "file.txt", account: account)
        var nfdItem = SendableItemMetadata(ocId: "nfd", fileName: "file.txt", account: account)
        nfcItem.serverUrl = "\(account.davFilesUrl)/pr\u{00EA}t"
        nfdItem.serverUrl = "\(account.davFilesUrl)/pre\u{0302}t"

        #expect(nfcItem.hasSameLocation(as: nfdItem))
    }

    @Test func canonicalEquivalentRemotePathsCompareEqual() {
        let account = Account(user: "user", id: "id", serverUrl: "https://examplecloud.com", password: "bla")
        var item = SendableItemMetadata(ocId: "item", fileName: "pr\u{00EA}t.pdf", account: account)
        item.serverUrl = account.davFilesUrl

        #expect(item.hasSameRemotePath(as: "\(account.davFilesUrl)/pre\u{0302}t.pdf"))
    }

    @Test func canonicalEquivalentRemotePathsIdentifyDescendants() {
        let account = Account(user: "user", id: "id", serverUrl: "https://examplecloud.com", password: "bla")
        var item = SendableItemMetadata(ocId: "item", fileName: "document.txt", account: account)
        item.serverUrl = "\(account.davFilesUrl)/pre\u{0302}t"

        #expect(item.isDescendant(of: "\(account.davFilesUrl)/pr\u{00EA}t"))
    }
}
