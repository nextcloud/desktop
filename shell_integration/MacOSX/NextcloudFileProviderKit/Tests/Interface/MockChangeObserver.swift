//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

public class MockChangeObserver: NSObject, NSFileProviderChangeObserver {
    public var changedItems: [any NSFileProviderItemProtocol] = []
    public var deletedItemIdentifiers: [NSFileProviderItemIdentifier] = []
    var error: Error?
    var isComplete = false
    var enumerator: NSFileProviderEnumerator

    public init(enumerator: NSFileProviderEnumerator) {
        self.enumerator = enumerator
    }

    public func didUpdate(_ changedItems: [any NSFileProviderItemProtocol]) {
        self.changedItems.append(contentsOf: changedItems)
    }

    public func didDeleteItems(withIdentifiers deletedItemIdentifiers: [NSFileProviderItemIdentifier]) {
        self.deletedItemIdentifiers.append(contentsOf: deletedItemIdentifiers)
    }

    public func finishEnumeratingChanges(upTo _: NSFileProviderSyncAnchor, moreComing _: Bool) {
        isComplete = true
    }

    public func finishEnumeratingWithError(_ error: Error) {
        self.error = error
        isComplete = true
    }

    public func enumerateChanges(from anchor: NSFileProviderSyncAnchor =
        .init(
            ISO8601DateFormatter()
                .string(from: Date(timeIntervalSince1970: 1))
                .data(using: .utf8)!
        )
    ) async throws {
        enumerator.enumerateChanges?(for: self, from: anchor)
        while !isComplete {
            try await Task.sleep(nanoseconds: 1_000_000)
        }
        if let error {
            throw error
        }
    }

    public func reset() {
        changedItems = []
        deletedItemIdentifiers = []
        error = nil
        isComplete = false
    }
}
