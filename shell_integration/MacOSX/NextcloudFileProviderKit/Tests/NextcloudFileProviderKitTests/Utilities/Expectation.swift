//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Testing

///
/// Concurrency safe expectation implementation inspired by the XCTest framework.
///
/// Usually, the Swift Testing confirmations are used but some use cases of testing actor states concurrently require something like this instead.
///
actor Expectation {
    ///
    /// Human-readable description of the explanation to make more sense of how it is being used.
    ///
    let description: String

    ///
    /// Present state of the expectation.
    ///
    private(set) var isFulfilled = false

    init(_ description: String) {
        self.description = description
    }

    ///
    /// Changes the present state to be fulfilled, if not already.
    ///
    /// Records an issue in case of overfulfillment.
    ///
    func fulfill(sourceLocation: SourceLocation = #_sourceLocation) {
        guard !isFulfilled else {
            Issue.record("Overfulfillment of expectation: \(description)", sourceLocation: sourceLocation)
            return
        }

        isFulfilled = true
    }
}
