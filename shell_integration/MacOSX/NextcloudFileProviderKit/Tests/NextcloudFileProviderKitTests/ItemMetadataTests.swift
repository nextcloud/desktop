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
}
