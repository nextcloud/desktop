//
//  MockEnumerationObserver.swift
//
//
//  Created by Claudio Cambra on 14/5/24.
//

import FileProvider
import Foundation

public class MockEnumerationObserver: NSObject, NSFileProviderEnumerationObserver {
    public var items: [NSFileProviderItem] = []
    private var error: Error?
    private var isComplete = false
    private var enumerator: NSFileProviderEnumerator

    public init(enumerator: NSFileProviderEnumerator) {
        self.enumerator = enumerator
    }

    public func didEnumerate(_ items: [NSFileProviderItem]) {
        self.items.append(contentsOf: items)
    }

    public func finishEnumerating(upTo nextPage: NSFileProviderPage?) {
        isComplete = true
    }

    public func finishEnumeratingWithError(_ error: Error) {
        self.error = error
        isComplete = true
    }

    public func enumerateItems() async throws {
        let startPage = NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage
        enumerator.enumerateItems(for: self, startingAt: startPage)
        while !isComplete {
            try await Task.sleep(nanoseconds: 1_000_000)
        }
        if let error {
            throw error
        }
    }
}
