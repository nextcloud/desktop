//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// One scenario's measurements.
///
/// Durations and counts are reported side by side on purpose. A duration alone cannot distinguish
/// code that got smarter from a server that was warmer; the counts are what a reviewer can hold the
/// claim to, and they are identical on every machine.
///
struct BenchmarkResult: Codable {
    let scenario: String
    let revision: String
    var metrics: [Metric]

    struct Metric: Codable {
        let name: String
        let value: Double
        let unit: String

        /// Whether a lower value is the improvement. Used when rendering a comparison.
        let lowerIsBetter: Bool
    }

    mutating func record(
        _ name: String, _ value: Double, unit: String, lowerIsBetter: Bool = true
    ) {
        metrics.append(Metric(name: name, value: value, unit: unit, lowerIsBetter: lowerIsBetter))
    }

    mutating func record(_ name: String, _ value: Int, unit: String, lowerIsBetter: Bool = true) {
        record(name, Double(value), unit: unit, lowerIsBetter: lowerIsBetter)
    }

    mutating func record(_ name: String, _ duration: Duration, lowerIsBetter: Bool = true) {
        record(name, duration.seconds, unit: "s", lowerIsBetter: lowerIsBetter)
    }
}

extension Duration {
    var seconds: Double {
        Double(components.seconds) + Double(components.attoseconds) * 1e-18
    }
}

///
/// Median of several runs, so one unlucky pass does not become the reported number.
///
func median(_ values: [Double]) -> Double {
    guard !values.isEmpty else { return 0 }
    let sorted = values.sorted()
    let middle = sorted.count / 2
    return sorted.count.isMultiple(of: 2)
        ? (sorted[middle - 1] + sorted[middle]) / 2
        : sorted[middle]
}
