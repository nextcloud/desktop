//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import Testing

///
/// The one loop which runs the passes of an enumeration scenario, so none can get the warm-ups, the repetitions or the counted region wrong.
///
enum Benchmark {
    /// Uncounted passes first, so state a scenario carries from one pass to the next, such as its sync anchor, settles.
    static let warmUpCount = 2

    /// Two counted passes are enough to check that every pass does the same work.
    static let passCount = 2

    /// Where the databases of these tests record their store version, kept out of the test host's standard defaults.
    static var defaults: UserDefaults {
        UserDefaults(suiteName: "com.nextcloud.NextcloudFileProviderKitPerformanceTests")!
    }

    ///
    /// Run the warm-ups and counted passes of one scenario and check that every counted pass did the same work.
    ///
    /// - Parameters:
    ///   - name: The scenario's name in failure messages.
    ///   - prepare: Uncounted setup for one pass, such as a fresh database or a change on the server.
    ///   - counts: The running totals of the pass's requests and database writes.
    ///   - run: The counted pass; returns how many items it reported.
    ///   - cleanUp: After every pass, also when it failed.
    ///
    /// - Returns: The counts of one counted pass.
    ///
    static func measure<Pass>(
        _ name: String,
        prepare: () async throws -> Pass,
        counts: (Pass) -> BenchmarkCounts,
        run: (Pass) async throws -> Int,
        cleanUp: (Pass) -> Void = { _ in }
    ) async throws -> BenchmarkCounts {
        var passCounts: [BenchmarkCounts] = []

        for pass in 0 ..< warmUpCount + passCount {
            let state = try await prepare()

            defer {
                cleanUp(state)
            }

            let before = counts(state)
            let reported = try await run(state)
            // The enumerator finishes its last batch on the main actor; that work belongs to the pass.
            await MainActor.run {}
            var difference = counts(state) - before
            difference.reported = reported

            if pass >= warmUpCount {
                passCounts.append(difference)
            }
        }

        #expect(Set(passCounts).count == 1, "Every pass of \(name) must do the same work: \(passCounts)")
        return passCounts[0]
    }

    ///
    /// Sleep into the next wall-clock second.
    ///
    /// Sync anchors carry whole seconds while rows carry exact times, so a row written in the second an anchor names counts as changed after it. Waiting before taking an anchor, or before a pass mints one, keeps earlier writes out of a pass.
    ///
    static func waitForNextSecond() async throws {
        let now = Date().timeIntervalSince1970
        try await Task.sleep(for: .seconds(now.rounded(.down) + 1 - now) + .milliseconds(1))
    }
}
