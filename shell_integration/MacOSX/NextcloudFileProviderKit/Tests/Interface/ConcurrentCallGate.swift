// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

/// Holds calls until `width` of them have been in flight together and for a short hold after that, so a call beyond the width would be counted too, and records the most calls seen in flight at once.
public actor ConcurrentCallGate {
    public static let defaultTimeout = Duration.seconds(10)
    private static let holdAfterWidth = Duration.milliseconds(100)
    private static let pollInterval = Duration.milliseconds(1)

    private let width: Int
    private let timeout: Duration
    private var deadline: ContinuousClock.Instant?
    private var releaseTime: ContinuousClock.Instant?
    private var inFlight = 0

    /// The most calls that were in flight at once.
    public private(set) var peak = 0

    /// - Parameter timeout: How long after the first call every call is let through, whether or not `width` was reached.
    public init(width: Int, timeout: Duration = defaultTimeout) {
        self.width = width
        self.timeout = timeout
    }

    public func pass() async {
        let deadline = deadline ?? .now + timeout
        self.deadline = deadline
        inFlight += 1
        peak = max(peak, inFlight)

        if peak >= width, releaseTime == nil {
            releaseTime = .now + Self.holdAfterWidth
        }

        while ContinuousClock.now < min(releaseTime ?? deadline, deadline), !Task.isCancelled {
            try? await Task.sleep(for: Self.pollInterval)
        }

        inFlight -= 1
    }
}
