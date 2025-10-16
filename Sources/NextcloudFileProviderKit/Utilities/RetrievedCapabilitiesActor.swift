//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudCapabilitiesKit

let CapabilitiesFetchInterval: TimeInterval = 30 * 60 // 30mins

actor RetrievedCapabilitiesActor {
    static let shared: RetrievedCapabilitiesActor = {
        let instance = RetrievedCapabilitiesActor()
        return instance
    }()

    var ongoingFetches: Set<String> = []
    var data: [String: (capabilities: Capabilities, retrievedAt: Date)] = [:]

    private var ongoingFetchContinuations: [String: [CheckedContinuation<Void, Never>]] = [:]

    func setCapabilities(
        forAccount account: String,
        capabilities: Capabilities,
        retrievedAt: Date = Date()
    ) {
        data[account] = (capabilities: capabilities, retrievedAt: retrievedAt)
    }

    func setOngoingFetch(forAccount account: String, ongoing: Bool) {
        if ongoing {
            ongoingFetches.insert(account)
        } else {
            ongoingFetches.remove(account)
            // If there are any continuations waiting for this account, resume them.
            if let continuations = ongoingFetchContinuations.removeValue(forKey: account) {
                continuations.forEach { $0.resume() }
            }
        }
    }

    func awaitFetchCompletion(forAccount account: String) async {
        guard ongoingFetches.contains(account) else { return }

        // If a fetch is ongoing, create a continuation and store it.
        await withCheckedContinuation { continuation in
            var existingContinuations = ongoingFetchContinuations[account, default: []]
            existingContinuations.append(continuation)
            ongoingFetchContinuations[account] = existingContinuations
        }
    }

    func reset() {
        ongoingFetches = []
        ongoingFetchContinuations = [:]
        data = [:]
    }
}
