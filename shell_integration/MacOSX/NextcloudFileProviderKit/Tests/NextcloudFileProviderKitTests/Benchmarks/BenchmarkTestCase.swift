//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudFileProviderKit
import XCTest

///
/// Base class for timing benchmarks, which run only when `NCFPK_BENCHMARKS=1` is set and print one `PERF-BENCH` line per measurement.
///
class BenchmarkTestCase: NextcloudFileProviderKitTestCase {
    static let account = Account(
        user: "benchUser", id: "benchUserId", serverUrl: "https://bench.nc.com", password: "abcd"
    )

    static let environmentKey = "NCFPK_BENCHMARKS"
    static let iterationCount = 5

    override func setUpWithError() throws {
        try super.setUpWithError()
        try XCTSkipUnless(
            ProcessInfo.processInfo.environment[Self.environmentKey] == "1",
            "Benchmarks run only with \(Self.environmentKey)=1."
        )
    }

    ///
    /// Print the median and standard deviation of `durations` on one `PERF-BENCH` line.
    ///
    /// - Returns: The median in seconds, or `nil` when there are no durations.
    ///
    @discardableResult
    func report(_ name: String, items: Int, durations: [Duration]) -> Double? {
        let seconds = durations.map { Double($0.components.seconds) + Double($0.components.attoseconds) / 1e18 }.sorted()
        guard !seconds.isEmpty else {
            return nil
        }

        let median = seconds[seconds.count / 2]
        let mean = seconds.reduce(0, +) / Double(seconds.count)
        let variance = seconds.reduce(0) { $0 + ($1 - mean) * ($1 - mean) } / Double(seconds.count)
        let itemsPerSecond = median > 0 ? Double(items) / median : 0
        print(String(format: "PERF-BENCH %@ items=%d iterations=%d median_s=%.3f stddev_s=%.3f items_per_s=%.0f", name, items, seconds.count, median, variance.squareRoot(), itemsPerSecond))
        return median
    }
}
