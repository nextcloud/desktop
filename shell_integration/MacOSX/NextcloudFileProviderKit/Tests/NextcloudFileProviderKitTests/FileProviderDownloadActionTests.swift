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
        accountIsChanging: Bool
    ) async throws {
        let domain = NSFileProviderDomain(identifier: .init(UUID().uuidString), displayName: "Download actions test")
        let fileProviderExtension = FileProviderExtension(domain: domain)
        defer { fileProviderExtension.invalidate() }
        if accountIsChanging {
            let manager = DatabaseTestSuites.makeManager()
            try #require(fileProviderExtension.publishDatabaseManager(
                manager,
                generation: fileProviderExtension.setupGeneration
            ))
            manager.syncOperations.stopAcceptingWork()
        }

        let error: (any Error)? = await withCheckedContinuation { continuation in
            _ = fileProviderExtension.performAction(
                identifier: action,
                onItemsWithIdentifiers: [.init("cached-item")]
            ) { error in
                continuation.resume(returning: error)
            }
        }

        let expected: NSFileProviderError.Code = accountIsChanging ? .serverUnreachable : .notAuthenticated
        #expect((error as? NSFileProviderError)?.code == expected)
    }
}
