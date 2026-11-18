//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Prints one `PERF-BENCH` line per measurement: the median and standard deviation of the recorded durations, the throughput, and optionally the number of server requests one pass issued.
///
enum BenchmarkReport {
    static func print(_ name: String, items: Int, durations: [Duration], requests: Int? = nil) {
        let seconds = durations.map { Double($0.components.seconds) + Double($0.components.attoseconds) / 1e18 }.sorted()
        guard !seconds.isEmpty else {
            return
        }

        let median = seconds[seconds.count / 2]
        let mean = seconds.reduce(0, +) / Double(seconds.count)
        let variance = seconds.reduce(0) { $0 + ($1 - mean) * ($1 - mean) } / Double(seconds.count)
        let itemsPerSecond = median > 0 ? Double(items) / median : 0
        let requestsText = requests.map { " requests=\($0)" } ?? ""
        Swift.print(String(format: "PERF-BENCH %@ items=%d%@ iterations=%d median_s=%.3f stddev_s=%.3f items_per_s=%.0f", name, items, requestsText, seconds.count, median, variance.squareRoot(), itemsPerSecond))
    }
}
