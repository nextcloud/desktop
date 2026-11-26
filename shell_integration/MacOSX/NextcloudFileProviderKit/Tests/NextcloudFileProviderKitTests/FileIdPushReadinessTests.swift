// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct FileIdPushReadinessTests {
    @Test("An ID push received before account setup requests a retry.")
    func notReadyRequestsRetry() {
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(UUID().uuidString), displayName: "Test")
        let fileProvider = FileProviderExtension(domain: domain)

        var replies = [Bool]()
        fileProvider.processFileIdsChanged([17]) { replies.append($0) }
        #expect(replies == [false])
    }
}
