//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import XCTest

///
/// A concurrency-safe counter for the sequential fulfillment of multiple expectations.
///
actor ExpectationFulfillmentCounter {
    ///
    /// The number of increments during the lifetime of this object.
    ///
    private(set) var count = 0

    let expectations: [XCTestExpectation]

    init(_ expectations: XCTestExpectation...) {
        self.expectations = expectations
    }

    ///
    /// Increase the state by one.
    ///
    func next(file: StaticString = #filePath, line: UInt = #line) {
        guard expectations.count > count else {
            XCTFail("Insufficient expectations to fulfill!", file: file, line: line)
            return
        }

        expectations[count].fulfill()
        count += 1
    }
}
