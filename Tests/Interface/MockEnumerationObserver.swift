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
    public var observedPages: [NSFileProviderPage] = []
    private var error: Error?
    private var isComplete = false
    private var currentPageComplete = false
    private var enumerator: NSFileProviderEnumerator
    private var page: NSFileProviderPage? = nil

    public init(enumerator: NSFileProviderEnumerator) {
        self.enumerator = enumerator
    }

    public func didEnumerate(_ items: [NSFileProviderItem]) {
        self.items.append(contentsOf: items)
    }

    public func finishEnumerating(upTo nextPage: NSFileProviderPage?) {
        page = nextPage
        isComplete = page == nil
        currentPageComplete = true
    }

    public func finishEnumeratingWithError(_ error: Error) {
        self.error = error
        isComplete = true
        currentPageComplete = true
    }

    public func enumerateItems() async throws {
        isComplete = false
        currentPageComplete = false
        observedPages = []
        page = NSFileProviderPage.initialPageSortedByName as NSFileProviderPage

        while let page, !isComplete {
            observedPages.append(page)
            enumerator.enumerateItems(for: self, startingAt: page)
            while !currentPageComplete {
                try await Task.sleep(nanoseconds: 1_000_000)
            }
            if let error {
                throw error
            }
            currentPageComplete = false
        }
    }
}
