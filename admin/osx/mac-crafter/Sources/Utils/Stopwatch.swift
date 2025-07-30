// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2025 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// A simple facility to record the run time of different execution phases of mac-crafter.
///
final class Stopwatch {
    private let formatter: NumberFormatter
    private var phases = [(name: String, date: Date)]()

    init() {
        formatter = NumberFormatter()
        formatter.minimumFractionDigits = 1
        formatter.maximumFractionDigits = 1
    }

    ///
    /// Add a new phase.
    ///
    func record(_ name: String) {
        phases.append((name: name, date: Date()))
    }

    ///
    /// Safely format a time interval as a seconds string.
    ///
    func format(_ duration: TimeInterval) -> String {
        "\(formatter.string(from: duration as NSNumber) ?? "nil")s"
    }

    ///
    /// Get a pretty printed string for all recorded phases.
    ///
    func report() -> String {
        var report = "Stopwatch recorded these phases and durations:\n"

        guard let beginning = phases.first?.date else {
            report.append("None")
            return report
        }

        for i in 0..<phases.count {
            let phase = phases[i]
            let nextDate = i + 1 < phases.count ? phases[i + 1].date : Date()
            let duration = nextDate.timeIntervalSince(phase.date)
            report.append("\t\(phase.name): \(format(duration))\n")
        }

        report.append("\tTotal: \(format(Date().timeIntervalSince(beginning)))\n")

        return report
    }
}
