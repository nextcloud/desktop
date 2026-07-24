//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct ItemMetadataTests {
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
