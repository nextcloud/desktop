//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Everything one working-set scan pass accumulates while it walks the directory tree.
///
/// Passed `inout` through the recursion. The children of one listing are walked together, each with a state
/// of its own seeded by ``forSubtree()``, and what they accumulated is taken over by ``merge(_:)`` in the order
/// the children were listed, so what a pass reports does not depend on which subtree finished first.
///
struct WorkingSetScanState {
    /// The remote state at which each directory was reconciled by an earlier pass, keyed by identifier.
    var scannedRemoteStates: [String: String]

    /// After this moment no listing is started any more; see ``WorkingSetScanLimits``.
    let deadline: ContinuousClock.Instant

    /// Whether the pass stopped starting listings at its deadline, with directories left to list.
    var stoppedAtDeadline = false

    /// The directories whose listings tombstoned something, and their ancestors. A pass which stopped early cannot tell a deletion from a move into a part it did not reach, so it revives the tombstones and forgets these, and the next pass decides.
    var tombstoningChains: Set<String> = []

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

    ///
    /// A state for the subtree below one child, carrying what the pass has decided but nothing it accumulated.
    ///
    /// The directories listed so far come along, so a subtree does not list again what an ancestor or an
    /// earlier sibling of an ancestor already listed. Two subtrees walked at the same time cannot see each
    /// other's listings, so a directory which moved into both of them during the pass is listed by each; the
    /// listings agree, and the pass reports the item once.
    ///
    func forSubtree() -> WorkingSetScanState {
        var subtree = WorkingSetScanState(scannedRemoteStates: scannedRemoteStates, deadline: deadline)
        subtree.listedOcIds = listedOcIds
        return subtree
    }

    /// Take over what the subtree below one child accumulated.
    mutating func merge(_ subtree: WorkingSetScanState) {
        stoppedAtDeadline = stoppedAtDeadline || subtree.stoppedAtDeadline
        tombstoningChains.formUnion(subtree.tombstoningChains)
        listedOcIds.formUnion(subtree.listedOcIds)
        createdAndUpdated.append(contentsOf: subtree.createdAndUpdated)
        survivingOcIds.formUnion(subtree.survivingOcIds)
        missing.append(contentsOf: subtree.missing)
        notFoundOcIds.formUnion(subtree.notFoundOcIds)
        reconciled.merge(subtree.reconciled) { _, fromSubtree in fromSubtree }
        forgotten.formUnion(subtree.forgotten)
        listedCount += subtree.listedCount
        skippedCount += subtree.skippedCount
        failedCount += subtree.failedCount
    }
}
