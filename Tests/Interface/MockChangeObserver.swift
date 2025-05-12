//
//  MockChangeObserver.swift
//
//
//  Created by Claudio Cambra on 15/5/24.
//

import FileProvider
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

    public func finishEnumeratingChanges(upTo anchor: NSFileProviderSyncAnchor, moreComing: Bool) {
        isComplete = true
    }

    public func finishEnumeratingWithError(_ error: Error) {
        self.error = error
        isComplete = true
    }

    public func enumerateChanges() async throws {
        let anchor = NSFileProviderSyncAnchor(.init())
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
