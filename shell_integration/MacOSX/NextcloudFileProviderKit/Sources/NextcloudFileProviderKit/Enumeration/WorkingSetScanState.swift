//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Everything one working-set scan pass accumulates while it walks the directory tree.
///
/// Passed `inout` through the recursion, so nothing is captured by a closure and the pass stays one task.
///
struct WorkingSetScanState {
    /// The remote state at which each directory was reconciled by an earlier pass, keyed by identifier.
    var scannedRemoteStates: [String: String]

    /// Directories listed in this pass, so a directory reached twice is listed once.
    var listedOcIds: Set<String> = []

    /// Rows the listings reported as created or updated, in listing order.
    var createdAndUpdated: [SendableItemMetadata] = []

    /// Identifiers seen alive somewhere in this pass; a tombstone with one of these was a move, not a deletion.
    var survivingOcIds: Set<String> = []

    /// Tombstones the listings wrote, and directories whose own listing answered 404.
    var missing: [SendableItemMetadata] = []

    /// The directories whose own listing answered 404, where the path the parent gave may be stale.
    var notFoundOcIds: Set<String> = []

    /// Directories reconciled in this pass with the state to remember, committed together after the pass.
    var reconciled: [String: String] = [:]

    /// Directories whose listing failed, and their ancestors: forgotten so the next pass descends there again.
    var forgotten: Set<String> = []

    var listedCount = 0
    var skippedCount = 0
    var failedCount = 0
}
