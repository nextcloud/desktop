//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing
import TestInterface

///
/// Coverage for the store identity in sync anchors: an anchor handed out before the metadata database was replaced must expire, one from the same file must not.
///
struct StoreReplacementAnchorTests {
    let directory: URL
    let domain = NSFileProviderDomainIdentifier("store-replacement-\(UUID().uuidString)")

    init() throws {
        directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("StoreReplacementAnchorTests-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
    }

    private func openManager() throws -> FilesDatabaseManager {
        try FilesDatabaseManager(
            account: DatabaseTestSuites.account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: domain,
            log: FileProviderLogMock(),
            defaults: DatabaseTestSuites.defaults
        )
    }

    private func makeWorkingSetEnumerator(on manager: FilesDatabaseManager) throws -> Enumerator {
        let account = DatabaseTestSuites.account
        return try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: account,
            remoteInterface: MockRemoteInterface(account: account, rootItem: MockRemoteItem.rootItem(account: account)),
            dbManager: manager,
            log: FileProviderLogMock()
        )
    }

    @Test func anAnchorFromAReplacedCorruptStoreIsExpired() async throws {
        let earlier = try openManager()
        let anchorFromEarlier = try makeWorkingSetEnumerator(on: earlier).currentAnchor
        earlier.checkpointForShutdown()
        try Data((0 ..< 8192).map { _ in UInt8.random(in: 0 ... 255) }).write(to: #require(earlier.databaseURL))

        let replacement = try openManager()
        #expect(replacement.storeIdentity != earlier.storeIdentity)
        let observer = try MockChangeObserver(enumerator: makeWorkingSetEnumerator(on: replacement))

        var caught: (any Error)?
        do {
            try await observer.enumerateChanges(from: anchorFromEarlier)
        } catch {
            caught = error
        }

        #expect((caught as? NSFileProviderError)?.code == .syncAnchorExpired)
        #expect(observer.finishes.isEmpty)
        #expect(observer.changedItems.isEmpty)
    }

    @Test func anAnchorFromTheSameStoreReopenedIsAccepted() async throws {
        let earlier = try openManager()
        let anchorFromEarlier = try makeWorkingSetEnumerator(on: earlier).currentAnchor
        earlier.checkpointForShutdown()

        let reopened = try openManager()
        #expect(reopened.storeIdentity == earlier.storeIdentity)
        let enumerator = try makeWorkingSetEnumerator(on: reopened)
        let observer = MockChangeObserver(enumerator: enumerator)

        try await observer.enumerateChanges(from: anchorFromEarlier)

        #expect(observer.finishes.last?.anchor.rawValue == enumerator.currentAnchor.rawValue)
    }
}
