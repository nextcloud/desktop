//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation

///
/// Change observer that drives an enumerator to completion and timestamps what it receives.
///
/// The File Provider framework supplies the real observer inside a running extension, so a
/// benchmark has to stand in for it. Unlike the server, this part cannot be real: there is no
/// framework observer outside an extension host. It does what the framework does — drain
/// `moreComing` batches until the enumerator reports the final one — and records when each batch
/// arrived, which is what makes time-to-first-change measurable.
///
final class BenchmarkChangeObserver: NSObject, NSFileProviderChangeObserver {
    private let lock = NSLock()
    private var _updated: [NSFileProviderItem] = []
    private var _deleted: [NSFileProviderItemIdentifier] = []
    private var _firstUpdateAt: ContinuousClock.Instant?
    private var _finished = false
    private var _error: Error?
    private var _anchor: NSFileProviderSyncAnchor?
    private var _moreComing = false
    private var _batchCount = 0

    /// Mirrors the batch size the framework suggests. `@objc` so the optional protocol requirement
    /// is visible to the production code through the protocol existential.
    @objc var suggestedBatchSize: Int = 0

    private let enumerator: NSFileProviderEnumerator
    private let startedAt: ContinuousClock.Instant

    init(enumerator: NSFileProviderEnumerator, startedAt: ContinuousClock.Instant) {
        self.enumerator = enumerator
        self.startedAt = startedAt
    }

    // MARK: - Results

    var updatedItems: [NSFileProviderItem] {
        lock.lock()
        defer { lock.unlock() }
        return _updated
    }

    var deletedIdentifiers: [NSFileProviderItemIdentifier] {
        lock.lock()
        defer { lock.unlock() }
        return _deleted
    }

    var batchCount: Int {
        lock.lock()
        defer { lock.unlock() }
        return _batchCount
    }

    /// How long after the enumeration started the first change reached the observer.
    var timeToFirstChange: Duration? {
        lock.lock()
        defer { lock.unlock() }
        guard let _firstUpdateAt else { return nil }
        return startedAt.duration(to: _firstUpdateAt)
    }

    var error: Error? {
        lock.lock()
        defer { lock.unlock() }
        return _error
    }

    // MARK: - NSFileProviderChangeObserver

    func didUpdate(_ updatedItems: [NSFileProviderItem]) {
        lock.lock()
        defer { lock.unlock() }
        if _firstUpdateAt == nil, !updatedItems.isEmpty {
            _firstUpdateAt = ContinuousClock.now
        }
        _updated.append(contentsOf: updatedItems)
    }

    func didDeleteItems(withIdentifiers deletedItemIdentifiers: [NSFileProviderItemIdentifier]) {
        lock.lock()
        defer { lock.unlock() }
        _deleted.append(contentsOf: deletedItemIdentifiers)
    }

    func finishEnumeratingChanges(upTo anchor: NSFileProviderSyncAnchor, moreComing: Bool) {
        lock.lock()
        defer { lock.unlock() }
        _anchor = anchor
        _moreComing = moreComing
        _batchCount += 1
        _finished = !moreComing
    }

    func finishEnumeratingWithError(_ error: Error) {
        lock.lock()
        defer { lock.unlock() }
        _error = error
        _finished = true
    }

    // MARK: - Driving

    /// `NSLock` cannot be taken from an async context, so the drive loop reads state through these.
    private func beginBatch() {
        lock.lock()
        defer { lock.unlock() }
        _finished = false
        _moreComing = false
    }

    private func batchState() -> (
        finished: Bool, moreComing: Bool, anchor: NSFileProviderSyncAnchor?, error: Error?
    ) {
        lock.lock()
        defer { lock.unlock() }
        return (_finished, _moreComing, _anchor, _error)
    }

    ///
    /// Request changes from `anchor` and keep requesting while the enumerator reports `moreComing`,
    /// exactly as the framework does.
    ///
    func enumerateChanges(from anchor: NSFileProviderSyncAnchor) async throws {
        var nextAnchor = anchor

        while true {
            beginBatch()
            enumerator.enumerateChanges?(for: self, from: nextAnchor)

            while true {
                let state = batchState()

                if let failure = state.error {
                    throw failure
                }
                if state.finished {
                    return
                }
                if state.moreComing, let continuation = state.anchor {
                    nextAnchor = continuation
                    break
                }

                try await Task.sleep(nanoseconds: 1_000_000)
            }
        }
    }
}

///
/// Item enumeration observer that pages through a container to completion.
///
final class BenchmarkEnumerationObserver: NSObject, NSFileProviderEnumerationObserver {
    private let lock = NSLock()
    private var _items: [NSFileProviderItem] = []
    private var _pageCount = 0
    private var _nextPage: NSFileProviderPage?
    private var _pageComplete = false
    private var _complete = false
    private var _error: Error?

    @objc var suggestedPageSize: Int = 0

    private let enumerator: NSFileProviderEnumerator

    init(enumerator: NSFileProviderEnumerator) {
        self.enumerator = enumerator
    }

    var items: [NSFileProviderItem] {
        lock.lock()
        defer { lock.unlock() }
        return _items
    }

    var pageCount: Int {
        lock.lock()
        defer { lock.unlock() }
        return _pageCount
    }

    func didEnumerate(_ updatedItems: [NSFileProviderItem]) {
        lock.lock()
        defer { lock.unlock() }
        _items.append(contentsOf: updatedItems)
    }

    func finishEnumerating(upTo nextPage: NSFileProviderPage?) {
        lock.lock()
        defer { lock.unlock() }
        _nextPage = nextPage
        _complete = nextPage == nil
        _pageComplete = true
        _pageCount += 1
    }

    func finishEnumeratingWithError(_ error: Error) {
        lock.lock()
        defer { lock.unlock() }
        _error = error
        _complete = true
        _pageComplete = true
    }

    /// `NSLock` cannot be taken from an async context, so the paging loop reads state through these.
    private func beginPaging() {
        lock.lock()
        defer { lock.unlock() }
        _complete = false
        _pageComplete = false
        _nextPage = NSFileProviderPage.initialPageSortedByName as NSFileProviderPage
    }

    private func pageToRequest() -> NSFileProviderPage? {
        lock.lock()
        defer { lock.unlock() }
        return _complete ? nil : _nextPage
    }

    private func pageState() -> (complete: Bool, error: Error?) {
        lock.lock()
        defer { lock.unlock() }
        return (_pageComplete, _error)
    }

    private func clearPageComplete() {
        lock.lock()
        defer { lock.unlock() }
        _pageComplete = false
    }

    func enumerateItems() async throws {
        beginPaging()

        while let page = pageToRequest() {
            enumerator.enumerateItems(for: self, startingAt: page)

            while true {
                let state = pageState()

                if let failure = state.error {
                    throw failure
                }
                if state.complete {
                    break
                }

                try await Task.sleep(nanoseconds: 1_000_000)
            }

            clearPageComplete()
        }
    }
}
