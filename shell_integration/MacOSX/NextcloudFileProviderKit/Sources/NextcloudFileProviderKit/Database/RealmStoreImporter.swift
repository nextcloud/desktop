//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB
import RealmSwift

///
/// One-time import of a domain's Realm database into the SQLite database which replaced it.
///
/// A Realm file next to the database means the last build which ran was still Realm-based, so its content wins: the import replaces any SQLite file. The import is written to a staging file and moved into place in one step, so a crash leaves either the untouched Realm file or a complete SQLite file behind, never a partial one. The Realm files are removed only after the move succeeded.
///
/// This type and the `LegacyRealm` models leave together with the Realm dependency one release after the switch.
///
enum RealmStoreImporter {
    enum Outcome: Equatable {
        /// No Realm file exists.
        case notNeeded
        /// The Realm file was imported and removed.
        case imported(rows: Int)
        /// The Realm file could not be read. It was set aside under ``failedSuffix`` and the database starts empty.
        case unreadable
        /// The import could not be written. The Realm file is untouched and the import is retried on the next start.
        case failed
    }

    /// Appended to the database path while the import is being written.
    static let stagingSuffix = ".importing"

    /// Appended to a Realm file which could not be read.
    static let failedSuffix = ".import-failed"

    /// Appended to an imported Realm file which could not be removed.
    static let importedSuffix = ".imported"

    /// The configuration the last Realm-based build used, so Realm upgrades older files on open before anything is read.
    static func legacyConfiguration(fileURL: URL) -> Realm.Configuration {
        Realm.Configuration(
            fileURL: fileURL,
            schemaVersion: LegacyRealmSchemaVersion.addedChangeDeliveryAcknowledgementState.rawValue,
            migrationBlock: { migration, oldSchemaVersion in
                if oldSchemaVersion == LegacyRealmSchemaVersion.initial.rawValue {
                    var localFileMetadataOcIds = Set<String>()

                    migration.enumerateObjects(ofType: "LocalFileMetadata") { oldObject, _ in
                        guard let oldObject, let lfmOcId = oldObject["ocId"] as? String else {
                            return
                        }

                        localFileMetadataOcIds.insert(lfmOcId)
                    }

                    migration.enumerateObjects(ofType: LegacyRealmItemMetadata.className()) { _, newObject in
                        guard let newObject,
                              let imOcId = newObject["ocId"] as? String,
                              localFileMetadataOcIds.contains(imOcId)
                        else { return }

                        newObject["downloaded"] = true
                        newObject["uploaded"] = true
                    }
                }

                if oldSchemaVersion < LegacyRealmSchemaVersion.addedCanonicalPathKeysToRealmItemMetadata.rawValue {
                    migration.enumerateObjects(ofType: LegacyRealmItemMetadata.className()) { _, newObject in
                        guard let newObject,
                              let serverUrl = newObject["serverUrl"] as? String,
                              let fileName = newObject["fileName"] as? String
                        else { return }

                        newObject["normalizedServerUrl"] = serverUrl.precomposedStringWithCanonicalMapping
                        newObject["normalizedFileName"] = fileName.precomposedStringWithCanonicalMapping
                    }
                }
            },
            objectTypes: [
                LegacyRealmItemMetadata.self,
                LegacyRealmExcludedFromSyncItem.self,
                LegacyRealmRemoteFileChunk.self,
                LegacyRealmPendingChunkUploadCleanup.self,
                LegacyRealmChangeDeliverySession.self,
                LegacyRealmChangeDeliveryItem.self
            ]
        )
    }

    ///
    /// Import the Realm file at `realmURL` into a new SQLite database at `sqliteURL`, if the Realm file exists.
    ///
    @discardableResult
    static func importIfNeeded(realmURL: URL, sqliteURL: URL, logger: FileProviderLogger) -> Outcome {
        let fileManager = FileManager.default

        guard fileManager.fileExists(atPath: realmURL.path) else {
            return .notNeeded
        }

        let stagingURL = URL(fileURLWithPath: sqliteURL.path + stagingSuffix)
        removeIfPresent(stagingURL.path)
        removeIfPresent(stagingURL.path + "-journal")

        let configuration = legacyConfiguration(fileURL: realmURL)
        logger.info("Importing the Realm database into the metadata database.", [.url: realmURL.path])

        let outcome: Outcome = autoreleasepool {
            let realm: Realm

            do {
                realm = try Realm(configuration: configuration)
            } catch {
                logger.fault("Could not open the Realm database for import. Setting it aside; the metadata database starts empty.", [.url: realmURL.path, .error: error])
                return .unreadable
            }

            defer {
                realm.invalidate()
            }

            do {
                let rows = try copy(realm, to: stagingURL)
                return .imported(rows: rows)
            } catch {
                logger.fault("Could not write the imported metadata database. The import is retried on the next start.", [.url: stagingURL.path, .error: error])
                return .failed
            }
        }

        switch outcome {
            case .unreadable:
                removeIfPresent(stagingURL.path)
                setAside(realmURL: realmURL, configuration: configuration, logger: logger)
                return outcome
            case .failed:
                removeIfPresent(stagingURL.path)
                return outcome
            case .notNeeded, .imported:
                break
        }

        do {
            for path in [sqliteURL.path, sqliteURL.path + "-wal", sqliteURL.path + "-shm"] {
                removeIfPresent(path)
            }

            try fileManager.moveItem(at: stagingURL, to: sqliteURL)
        } catch {
            logger.fault("Could not move the imported metadata database into place. The import is retried on the next start.", [.url: sqliteURL.path, .error: error])
            removeIfPresent(stagingURL.path)
            return .failed
        }

        deleteRealmFiles(configuration: configuration, logger: logger)
        logger.info("Imported the Realm database into the metadata database.", [.url: sqliteURL.path])

        return outcome
    }

    /// Copy every table into a fresh database at `stagingURL` in one transaction and return the number of rows written.
    private static func copy(_ realm: Realm, to stagingURL: URL) throws -> Int {
        var configuration = Configuration()
        configuration.label = "RealmStoreImporter"

        let queue = try DatabaseQueue(path: stagingURL.path, configuration: configuration)
        try DatabaseSchema.migrator.migrate(queue)

        return try queue.write { db in
            var rows = 0

            for row in realm.objects(LegacyRealmItemMetadata.self) {
                try autoreleasepool {
                    var record = ItemMetadataRecord(row)
                    // The keys are copied as stored so the startup repair treats them exactly as it treated the Realm rows.
                    record.normalizedServerUrl = row.normalizedServerUrl
                    record.normalizedFileName = row.normalizedFileName
                    try record.upsertRow(db)
                }
                rows += 1
            }

            for row in realm.objects(LegacyRealmExcludedFromSyncItem.self) {
                try ExcludedFromSyncItemRecord(ocId: row.ocId).insert(db)
                rows += 1
            }

            for row in realm.objects(LegacyRealmRemoteFileChunk.self) {
                try RemoteFileChunkRecord(row.chunk).insert(db, onConflict: .ignore)
                rows += 1
            }

            for row in realm.objects(LegacyRealmPendingChunkUploadCleanup.self) {
                try PendingChunkUploadCleanupRecord(uploadIdentifier: row.uploadIdentifier).insert(db)
                rows += 1
            }

            for row in realm.objects(LegacyRealmChangeDeliverySession.self) {
                try ChangeDeliverySessionRecord(
                    sessionId: row.sessionId,
                    containerKey: row.containerKey,
                    currentAnchorKey: row.currentAnchorKey,
                    nextSequence: row.nextSequence,
                    finalAnchorRawValue: row.finalAnchorRawValue,
                    incomplete: row.incomplete,
                    completed: row.completed,
                    pendingEndSequence: row.pendingEndSequence,
                    pendingAnchorKey: row.pendingAnchorKey,
                    pendingMoreComing: row.pendingMoreComing,
                    pendingReported: row.pendingReported,
                    hardRemoveDeleted: row.hardRemoveDeleted
                ).insert(db)
                rows += 1
            }

            for row in realm.objects(LegacyRealmChangeDeliveryItem.self) {
                try ChangeDeliveryItemRecord(
                    sessionId: row.sessionId,
                    sequence: row.sequence,
                    metadataData: row.metadataData,
                    deleted: row.deleted
                ).insert(db, onConflict: .ignore)
                rows += 1
            }

            try db.execute(sql: "PRAGMA user_version = \(StoreVersion.current)")

            return rows
        }
    }

    /// Rename an unreadable Realm file so it stays available for support and remove its auxiliary files.
    private static func setAside(realmURL: URL, configuration: Realm.Configuration, logger: FileProviderLogger) {
        do {
            try FileManager.default.moveItem(atPath: realmURL.path, toPath: realmURL.path + failedSuffix)
        } catch {
            logger.error("Could not set aside the unreadable Realm database.", [.url: realmURL.path, .error: error])
            return
        }

        deleteRealmFiles(configuration: configuration, logger: logger)
    }

    /// Remove the Realm file and its note, management and lock files.
    private static func deleteRealmFiles(configuration: Realm.Configuration, logger: FileProviderLogger) {
        do {
            _ = try Realm.deleteFiles(for: configuration)
        } catch {
            logger.error("Could not remove the imported Realm database files. They are removed on the next start.", [.url: configuration.fileURL?.path, .error: error])
        }

        // Realm leaves the lock file in place because another process could hold it; nothing else opens this domain's database.
        if let lockPath = configuration.fileURL.map({ $0.path + ".lock" }) {
            removeIfPresent(lockPath)
        }

        // A Realm file which survives here would be imported again on the next start and replace everything written until then.
        if let realmPath = configuration.fileURL?.path, FileManager.default.fileExists(atPath: realmPath) {
            do {
                try FileManager.default.moveItem(atPath: realmPath, toPath: realmPath + importedSuffix)
            } catch {
                logger.fault("Could not rename the imported Realm database. It will be imported again on the next start.", [.url: realmPath, .error: error])
            }
        }
    }

    private static func removeIfPresent(_ path: String) {
        guard FileManager.default.fileExists(atPath: path) else {
            return
        }

        try? FileManager.default.removeItem(atPath: path)
    }
}
