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
    let storeIdentity = UUID().uuidString

    @Test func anchorsCarryTheExtensionVersionStoreVersionAndIdentity() throws {
        let date = Date(timeIntervalSince1970: 1_700_000_000)

        let anchor = Enumerator.syncAnchor(at: date, storeIdentity: storeIdentity)
        let raw = try #require(String(data: anchor.rawValue, encoding: .utf8))

        #expect(raw.hasPrefix(Enumerator.currentExtensionVersion() + "+store\(StoreVersion.current)+\(storeIdentity)|"))
        let parsed = try #require(Enumerator.parseSyncAnchor(anchor))
        #expect(parsed.version == Enumerator.anchorVersionTag(storeIdentity: storeIdentity))
        #expect(parsed.date == date)
    }

    @Test func anchorsFromAnotherStoreVersionDoNotMatchTheRunningTag() throws {
        let raw = "\(Enumerator.currentExtensionVersion())+store\(StoreVersion.current + 1)+\(storeIdentity)|2024-01-01T00:00:00Z"
        let anchor = try NSFileProviderSyncAnchor(#require(raw.data(using: .utf8)))

        let parsed = try #require(Enumerator.parseSyncAnchor(anchor))
        #expect(parsed.version != Enumerator.anchorVersionTag(storeIdentity: storeIdentity))
    }

    @Test func anchorsFromAnotherStoreIdentityDoNotMatchTheRunningTag() throws {
        let anchor = Enumerator.syncAnchor(at: Date(), storeIdentity: UUID().uuidString)

        let parsed = try #require(Enumerator.parseSyncAnchor(anchor))
        #expect(parsed.version != Enumerator.anchorVersionTag(storeIdentity: storeIdentity))
    }

    @Test func anchorsWithoutAStoreIdentityDoNotMatchTheRunningTag() throws {
        // The tag of the builds which carried only the store version.
        let raw = "\(Enumerator.currentExtensionVersion())+store1|2024-01-01T00:00:00Z"
        let anchor = try NSFileProviderSyncAnchor(#require(raw.data(using: .utf8)))

        let parsed = try #require(Enumerator.parseSyncAnchor(anchor))
        #expect(parsed.version != Enumerator.anchorVersionTag(storeIdentity: storeIdentity))
    }
}
