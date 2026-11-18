//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Decides what to do with an existing metadata database given the store version it was last opened with.
///
/// The version a build last recorded is read from the domain's defaults and from the file itself; the higher one counts. A database newer than this build understands is removed, because an older build cannot tell which of its assumptions the newer format broke. So is a database older than ``StoreVersion/oldestMigratable``, because no migration brings it up to date.
///
enum StoreVersionGuard {
    enum Decision: Equatable {
        /// No version has been recorded: a new domain, or defaults and file both absent.
        case fresh
        /// The database is older than this build and is migrated on open.
        case upgrade(from: Int)
        /// The database is older than the oldest version this build migrates and must be set up from scratch.
        case obsolete(from: Int)
        /// The database matches this build.
        case current
        /// The database was written by a newer build and must be set up from scratch.
        case downgrade(from: Int)
    }

    static func decide(seen: Int?, current: Int = StoreVersion.current, oldestMigratable: Int = StoreVersion.oldestMigratable) -> Decision {
        guard let seen, seen > 0 else {
            return .fresh
        }

        if seen < oldestMigratable {
            return .obsolete(from: seen)
        }

        if seen < current {
            return .upgrade(from: seen)
        }

        if seen > current {
            return .downgrade(from: seen)
        }

        return .current
    }

    ///
    /// The store version recorded in an existing database file, or `nil` when there is no file or it cannot be read.
    ///
    /// A file carrying migrations this build does not know is reported as newer than any version this build has.
    ///
    static func recordedVersion(at url: URL) -> Int? {
        guard FileManager.default.fileExists(atPath: url.path) else {
            return nil
        }

        var configuration = Configuration()
        configuration.readonly = true
        configuration.label = "StoreVersionGuard"

        guard let queue = try? DatabaseQueue(path: url.path, configuration: configuration) else {
            return nil
        }

        return try? queue.read { db in
            if try DatabaseSchema.migrator.hasBeenSuperseded(db) {
                return Int.max
            }

            return try Int.fetchOne(db, sql: "PRAGMA user_version")
        }
    }

    ///
    /// Remove the database file together with its journal and any unfinished import next to it.
    ///
    /// The database file goes last, so a removal which fails or is interrupted leaves it in place, and the next open decides again from the version it records.
    ///
    /// - Throws: ``FilesDatabaseManager/OpenError/openFailed(_:underlying:)`` when a file could not be removed; opening the store anyway would keep a file this build cannot use.
    ///
    static func resetStore(at url: URL, logger: FileProviderLogger) throws {
        for path in [url.path + "-wal", url.path + "-shm", url.path + RealmStoreImporter.stagingSuffix, url.path] where FileManager.default.fileExists(atPath: path) {
            do {
                try FileManager.default.removeItem(atPath: path)
            } catch {
                logger.error("Could not remove a metadata database file while setting the store up from scratch.", [.url: path, .error: error])
                throw FilesDatabaseManager.OpenError.openFailed(url, underlying: error)
            }
        }
    }
}
