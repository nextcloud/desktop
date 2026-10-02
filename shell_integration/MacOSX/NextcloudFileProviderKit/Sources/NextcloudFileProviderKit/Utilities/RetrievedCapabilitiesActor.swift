//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudCapabilitiesKit

let CapabilitiesFetchInterval: TimeInterval = 30 * 60 // 30mins

actor RetrievedCapabilitiesActor: Sendable {
    static let shared = RetrievedCapabilitiesActor()

    var ongoingFetches: Set<String> = []
    private var data: [String: (capabilities: Capabilities, retrievedAt: Date)] = [:]

    private var ongoingFetchContinuations: [String: [UUID: CheckedContinuation<Void, Never>]] = [:]

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
        ongoingFetches = []
        ongoingFetchContinuations = [:]
        data = [:]
    }
}
