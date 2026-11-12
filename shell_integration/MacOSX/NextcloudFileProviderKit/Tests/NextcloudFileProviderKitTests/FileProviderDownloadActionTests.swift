// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct FileProviderDownloadActionTests {
    @Test(
        arguments: [
            NSFileProviderExtensionActionIdentifier.keepDownloaded,
            .evictAutomatically,
            .evict,
            .evictDescendants
        ],
        [false, true]
    )
    func reportsUnavailableDatabase(
        action: NSFileProviderExtensionActionIdentifier,
        managerWasInvalidated: Bool
    ) async throws {
        let domain = NSFileProviderDomain(identifier: .init(UUID().uuidString), displayName: "Download actions test")
        let fileProviderExtension = FileProviderExtension(domain: domain)
        defer { fileProviderExtension.invalidate() }
        if managerWasInvalidated {
            let manager = DatabaseTestSuites.makeManager()
            try #require(fileProviderExtension.publishDatabaseManager(
                manager,
                generation: fileProviderExtension.setupGeneration
            ))
            manager.invalidateForDAVAccountChange()
        }

        let error: (any Error)? = await withCheckedContinuation { continuation in
            _ = fileProviderExtension.performAction(
                identifier: action,
                onItemsWithIdentifiers: [.init("cached-item")]
            ) { error in
                continuation.resume(returning: error)
            }
        }

        #expect((error as? NSFileProviderError)?.code == .cannotSynchronize)
    }
}
