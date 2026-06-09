//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// Thread-safe TTL cache mapping the old remote path of a renamed directory to the
/// ocId of that directory at its new path.
///
/// When an editor (e.g. LibreOffice) holds a document open and the containing folder
/// is renamed on another client, the editor may continue writing to the old path.
/// macOS File Provider then calls ``FileProviderExtension.createItem`` for the old
/// folder name. Without this cache the extension would issue a MKCOL for the old
/// path, creating a duplicate folder on the server.
///
/// ``Item.createNewFolder`` consults this cache before calling MKCOL. If the
/// requested path was recently renamed, the existing (renamed) ``Item`` is returned
/// instead of creating a new folder. Entries expire after ``ttl`` seconds; beyond
/// that, the request is treated as genuinely new content.
///
public final class RecentRenameTracker: @unchecked Sendable {
    private struct Entry {
        let ocId: String
        let expiry: Date
    }

    private var entries: [String: Entry] = [:]
    private let lock = NSLock()
    let ttl: TimeInterval

    public init(ttl: TimeInterval = 60) {
        self.ttl = ttl
    }

    ///
    /// Record that the directory previously at ``oldPath`` has been renamed.
    ///
    /// - Parameters:
    ///   - oldPath: The full remote path the directory occupied before the rename.
    ///   - newOcId: The ocId of the directory at its new path (unchanged by the rename).
    ///
    public func record(oldPath: String, newOcId: String) {
        lock.withLock {
            prune()
            entries[oldPath] = Entry(ocId: newOcId, expiry: Date().addingTimeInterval(ttl))
        }
    }

    ///
    /// Return the ocId of the directory that was recently at ``oldPath``, or ``nil``
    /// if no unexpired entry exists for that path.
    ///
    public func ocId(for oldPath: String) -> String? {
        lock.withLock {
            prune()
            return entries[oldPath]?.ocId
        }
    }

    private func prune() {
        let now = Date()
        entries = entries.filter { $0.value.expiry > now }
    }
}
