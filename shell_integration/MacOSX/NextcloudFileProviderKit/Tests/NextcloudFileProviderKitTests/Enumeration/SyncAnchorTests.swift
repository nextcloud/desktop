//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import Testing

///
/// Coverage for the version tag embedded in sync anchors.
///
struct SyncAnchorTests {
    @Test func anchorsCarryTheExtensionAndStoreVersion() throws {
        let date = Date(timeIntervalSince1970: 1_700_000_000)

        let anchor = Enumerator.syncAnchor(at: date)
        let raw = try #require(String(data: anchor.rawValue, encoding: .utf8))

        #expect(raw.hasPrefix(Enumerator.currentExtensionVersion() + "+store\(StoreVersion.current)|"))
        let parsed = try #require(Enumerator.parseSyncAnchor(anchor))
        #expect(parsed.version == Enumerator.anchorVersionTag())
        #expect(parsed.date == date)
    }

    @Test func anchorsFromAnotherStoreVersionDoNotMatchTheRunningTag() throws {
        let raw = "\(Enumerator.currentExtensionVersion())+store\(StoreVersion.current + 1)|2024-01-01T00:00:00Z"
        let anchor = try NSFileProviderSyncAnchor(#require(raw.data(using: .utf8)))

        let parsed = try #require(Enumerator.parseSyncAnchor(anchor))
        #expect(parsed.version != Enumerator.anchorVersionTag())
    }
}
