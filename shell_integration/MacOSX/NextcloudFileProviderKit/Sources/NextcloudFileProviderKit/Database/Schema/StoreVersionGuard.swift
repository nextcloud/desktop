//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Decides what to do with an existing metadata database given the store version it was last opened with.
///
/// The version a build last recorded is read from the domain's defaults and from the file itself; the higher one counts. A database newer than this build understands is removed, because an older build cannot tell which of its assumptions the newer format broke.
///
enum StoreVersionGuard {
    enum Decision: Equatable {
        /// No version has been recorded: a new domain, or defaults and file both absent.
        case fresh
        /// The database is older than this build and is migrated on open.
        case upgrade(from: Int)
        /// The database matches this build.
        case current
        /// The database was written by a newer build and must be set up from scratch.
        case downgrade(from: Int)
    }

    static func decide(seen: Int?, current: Int = StoreVersion.current) -> Decision {
        guard let seen, seen > 0 else {
            return .fresh
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

    /// Remove the database file together with its journal and any unfinished import next to it.
    static func resetStore(at url: URL, logger: FileProviderLogger) {
        for path in [url.path, url.path + "-wal", url.path + "-shm", url.path + RealmStoreImporter.stagingSuffix] where FileManager.default.fileExists(atPath: path) {
            do {
                try FileManager.default.removeItem(atPath: path)
            } catch {
                logger.error("Could not remove a metadata database file while setting the store up from scratch.", [.url: path, .error: error])
            }
        }
    }
}
