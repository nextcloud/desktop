//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

public class MockEnumerationObserver: NSObject, NSFileProviderEnumerationObserver {
    public var items: [NSFileProviderItem] = []
    public var observedPages: [NSFileProviderPage] = []
    public private(set) var error: Error?
    public private(set) var page: NSFileProviderPage?
    private var isComplete = false
    private var currentPageComplete = false
    private var enumerator: NSFileProviderEnumerator

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
            try await enumerateItemsPage(page: page)
            observedPages.append(page)
        }
    }

    public func enumerateItemsPage(page: NSFileProviderPage) async throws {
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
