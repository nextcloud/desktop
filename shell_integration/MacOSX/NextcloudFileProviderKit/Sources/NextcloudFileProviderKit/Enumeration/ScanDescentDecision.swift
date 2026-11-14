//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// What the working-set scan does with a child directory its parent's listing reports.
///
/// The same rule decides the walk and whether a directory the system enumerated counts as reconciled, so the two never disagree.
///
enum ScanDescentDecision: Equatable {
    /// Nothing materialized is tracked below the directory; its own row is written and reported by the parent's listing already.
    case nothingTracked
    /// The directory's state equals the state at which the scan reconciled everything below it.
    case unchanged
    /// The directory has to be listed.
    case descend

    ///
    /// - Parameters:
    ///   - child: The directory as its parent's listing reports it.
    ///   - holdsMaterialisedItem: Whether a live materialized item is stored anywhere below it.
    ///   - holdsExternalStorage: Whether a live row of an external storage is stored anywhere below it.
    ///   - reconciledState: The state at which an earlier pass reconciled it, if any.
    ///
    init(child: SendableItemMetadata, holdsMaterialisedItem: Bool, holdsExternalStorage: Bool, reconciledState: String?) {
        // "Always keep downloaded" asks for the whole subtree, so a directory created on the server inside a pinned folder is listed although nothing inside it is known yet.
        guard child.visitedDirectory || child.keepDownloaded || holdsMaterialisedItem else {
            self = .nothingTracked
            return
        }

        // The server notices changes made behind its back on an external storage only when the directory is read, so one is listed on every pass, with everything above and below it.
        guard child.mountType != FilesDatabaseManager.externalMountType, !holdsExternalStorage else {
            self = .descend
            return
        }

        guard child.status == Status.normal.rawValue else {
            self = .descend
            return
        }

        // A directory never reconciled is listed once, whatever its etag: only the scan's own state says the subtree was checked.
        self = reconciledState == FilesDatabaseManager.scanRemoteState(of: child) ? .unchanged : .descend
    }
}
