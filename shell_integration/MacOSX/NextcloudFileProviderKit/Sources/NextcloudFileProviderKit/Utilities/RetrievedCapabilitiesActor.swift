//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit
import os

let CapabilitiesFetchInterval: TimeInterval = 30 * 60 // 30mins

actor RetrievedCapabilitiesActor: Sendable {
    typealias FetchCancellation = OSAllocatedUnfairLock<(task: URLSessionTask?, cancelled: Bool)>
    typealias FetchResult = (account: String, capabilities: Capabilities?, data: Data?, error: NKError)

    static let shared = RetrievedCapabilitiesActor()

    var ongoingFetches: Set<String> = []
    private var data: [String: (capabilities: Capabilities, retrievedAt: Date)] = [:]

    private var ongoingFetchContinuations: [String: [UUID: CheckedContinuation<Void, Never>]] = [:]
    private(set) var sharedFetches: [String: (identifier: UUID, task: Task<Void, Never>, cancellation: FetchCancellation)] = [:]
    private var sharedFetchContinuations: [String: [UUID: CheckedContinuation<FetchResult, Never>]] = [:]

    /// Share a fetch across callers and cancel its request when the last caller cancels.
    func currentCapabilities(
        forAccount account: String,
        onWaiting: @Sendable () -> Void = {},
        fetch: @Sendable @escaping (@Sendable @escaping (URLSessionTask) -> Void) async -> FetchResult
    ) async -> FetchResult {
        guard !Task.isCancelled else { return (account, nil, nil, .cancelled) }
        // Shared fetches have their own waiters; other tracked fetches use awaitFetchCompletion.
        if sharedFetches[account] == nil {
            await awaitFetchCompletion(forAccount: account, onWaiting: onWaiting)
        }
        let token = UUID()
        let result: FetchResult = await withTaskCancellationHandler {
            await withCheckedContinuation { continuation in
                guard !Task.isCancelled else {
                    continuation.resume(returning: (account, nil, nil, .cancelled))
                    return
                }
                if let cached = data[account], Date().timeIntervalSince(cached.retrievedAt) < CapabilitiesFetchInterval {
                    continuation.resume(returning: (account, cached.capabilities, nil, .success))
                    return
                }
                sharedFetchContinuations[account, default: [:]][token] = continuation
                onWaiting()
                guard sharedFetches[account] == nil else { return }
                let identifier = UUID()
                setOngoingFetch(forAccount: account, ongoing: true)
                // Registration may arrive after cancellation, so the callback shares a locked flag.
                let cancellation = FetchCancellation(initialState: (task: nil, cancelled: false))
                let task = Task.detached {
                    guard !Task.isCancelled else { return }
                    let result = await fetch { networkTask in
                        let shouldCancel = cancellation.withLock { state in
                            guard !state.cancelled else { return true }
                            state.task = networkTask
                            return false
                        }
                        if shouldCancel {
                            networkTask.cancel()
                        }
                    }
                    await self.finishSharedFetch(forAccount: account, identifier: identifier, result: result)
                }
                sharedFetches[account] = (identifier, task, cancellation)
            }
        } onCancel: {
            Task { await self.cancelSharedWaiter(forAccount: account, token: token) }
        }
        guard !Task.isCancelled else { return (account, nil, nil, .cancelled) }
        return result
    }

    private func finishSharedFetch(forAccount account: String, identifier: UUID, result: FetchResult) {
        guard sharedFetches[account]?.identifier == identifier else { return }
        if result.error == .success, let capabilities = result.capabilities {
            setCapabilities(forAccount: account, capabilities: capabilities)
        }
        sharedFetches.removeValue(forKey: account)
        let continuations = sharedFetchContinuations.removeValue(forKey: account)
        setOngoingFetch(forAccount: account, ongoing: false)
        continuations?.values.forEach { $0.resume(returning: result) }
    }

    private func cancelSharedWaiter(forAccount account: String, token: UUID) {
        guard let continuation = sharedFetchContinuations[account]?.removeValue(forKey: token) else { return }
        if sharedFetchContinuations[account]?.isEmpty == true {
            sharedFetchContinuations.removeValue(forKey: account)
            cancelSharedFetch(forAccount: account)
        }
        continuation.resume(returning: (account, nil, nil, .cancelled))
    }

    private func cancelSharedFetch(forAccount account: String) {
        guard let fetch = sharedFetches.removeValue(forKey: account) else { return }
        let networkTask = fetch.cancellation.withLock { state in
            state.cancelled = true
            return state.task
        }
        fetch.task.cancel()
        networkTask?.cancel()
        setOngoingFetch(forAccount: account, ongoing: false)
    }

    func getCapabilities(for account: String) -> (capabilities: Capabilities, retrievedAt: Date)? {
        data[account]
    }

    func setCapabilities(forAccount account: String, capabilities: Capabilities, retrievedAt: Date = Date()) {
        data[account] = (capabilities: capabilities, retrievedAt: retrievedAt)
    }

    func setOngoingFetch(forAccount account: String, ongoing: Bool) {
        if ongoing {
            ongoingFetches.insert(account)
        } else {
            ongoingFetches.remove(account)
            // If there are any continuations waiting for this account, resume them.
            if let continuations = ongoingFetchContinuations.removeValue(forKey: account) {
                continuations.values.forEach { $0.resume() }
            }
        }
    }

    func awaitFetchCompletion(forAccount account: String, onWaiting: @Sendable () -> Void = {}) async {
        guard !Task.isCancelled, ongoingFetches.contains(account) else { return }
        let token = UUID()
        await withTaskCancellationHandler {
            await withCheckedContinuation { continuation in
                guard !Task.isCancelled, ongoingFetches.contains(account) else {
                    continuation.resume()
                    return
                }
                ongoingFetchContinuations[account, default: [:]][token] = continuation
                onWaiting()
            }
        } onCancel: {
            Task { await self.cancelWaiter(forAccount: account, token: token) }
        }
    }

    private func cancelWaiter(forAccount account: String, token: UUID) {
        let continuation = ongoingFetchContinuations[account]?.removeValue(forKey: token)
        if ongoingFetchContinuations[account]?.isEmpty == true {
            ongoingFetchContinuations.removeValue(forKey: account)
        }
        continuation?.resume()
    }

    func reset() {
        for account in Array(sharedFetches.keys) {
            cancelSharedFetch(forAccount: account)
        }
        for (account, continuations) in sharedFetchContinuations {
            continuations.values.forEach { $0.resume(returning: (account, nil, nil, .cancelled)) }
        }
        sharedFetchContinuations = [:]
        ongoingFetchContinuations.values.forEach { $0.values.forEach { $0.resume() } }
        ongoingFetches = []
        ongoingFetchContinuations = [:]
        data = [:]
    }
}
