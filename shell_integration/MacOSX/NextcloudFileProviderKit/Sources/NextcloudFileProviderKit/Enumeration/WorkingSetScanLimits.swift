//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// How long one working-set pass may keep starting listings, and when to ask for the next pass after it stopped early.
///
/// The system issues no folder enumeration for a domain while a working-set change enumeration is outstanding, so a long pass freezes every Finder window on the domain for its duration. A pass which runs out of budget still takes the first descent below each directory it just listed, so it completes one chain, but starts no further sibling; it delivers what it found and reports no deletions, and once its last batch is acknowledged it has the working set signalled again after a delay, so the folder enumerations the system queued meanwhile get their turn first. The directories on the unfinished chains keep no reconciled state, so the next pass resumes there and skips what the previous one completed.
///
public struct WorkingSetScanLimits: Sendable {
    /// How long after the pass began it still starts a listing. The listing in flight at that moment finishes on its own, however long it takes.
    public let listingBudget: Duration

    /// How long after the last batch of a pass which stopped early is acknowledged the working set is signalled again.
    public let resumeDelay: Duration

    public init(listingBudget: Duration, resumeDelay: Duration) {
        self.listingBudget = listingBudget
        self.resumeDelay = resumeDelay
    }

    public static let `default` = WorkingSetScanLimits(listingBudget: .seconds(20), resumeDelay: .seconds(5))
}
