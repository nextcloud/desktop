//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Testing
import TestInterface

///
/// What one pass did, in figures which do not depend on the machine.
///
struct BenchmarkCounts: Hashable {
    /// PROPFIND requests sent to the server.
    var requests = 0

    /// Database transactions committed.
    var commits = 0

    /// SQLite rows inserted, updated or deleted.
    var rowChanges = 0

    /// Items reported to the system.
    var reported = 0

    init(requests: Int = 0, commits: Int = 0, rowChanges: Int = 0, reported: Int = 0) {
        self.requests = requests
        self.commits = commits
        self.rowChanges = rowChanges
        self.reported = reported
    }

    ///
    /// The running totals of a fixture's request recorder and database, to subtract from a later reading.
    ///
    init(recorder: EnumeratePathRecorder, writes: DatabaseWriteCounter) {
        self.init(requests: recorder.count, commits: writes.commits, rowChanges: writes.rowChanges())
    }

    static func - (lhs: Self, rhs: Self) -> Self {
        Self(
            requests: lhs.requests - rhs.requests,
            commits: lhs.commits - rhs.commits,
            rowChanges: lhs.rowChanges - rhs.rowChanges,
            reported: lhs.reported - rhs.reported
        )
    }

    ///
    /// Expect no count above its ceiling. A pull request which makes a count go up raises the ceiling and says why; one which makes it go down lowers it.
    ///
    func expectWithin(_ ceiling: Self, sourceLocation: SourceLocation = #_sourceLocation) {
        #expect(requests <= ceiling.requests, "requests", sourceLocation: sourceLocation)
        #expect(commits <= ceiling.commits, "commits", sourceLocation: sourceLocation)
        #expect(rowChanges <= ceiling.rowChanges, "row changes", sourceLocation: sourceLocation)
        #expect(reported <= ceiling.reported, "reported items", sourceLocation: sourceLocation)
    }
}
