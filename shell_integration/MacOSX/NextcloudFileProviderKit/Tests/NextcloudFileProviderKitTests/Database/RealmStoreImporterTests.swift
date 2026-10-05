//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import Testing

///
/// Coverage for the one-time import of a Realm database written before the switch to SQLite.
///
/// Serialized because Realm keeps process-wide caches per file.
///
@Suite("Realm store import", .serialized)
struct RealmStoreImporterTests {
    static let account = DatabaseTestSuites.account

    struct Fixture {
        let directory: URL
        let domain: NSFileProviderDomainIdentifier
        var realmURL: URL {
            directory.appendingPathComponent(domain.rawValue).appendingPathExtension("realm")
        }

        var sqliteURL: URL {
            directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension)
        }

        func openManager() -> FilesDatabaseManager {
            FilesDatabaseManager(account: account, databaseDirectory: directory, fileProviderDomainIdentifier: domain, log: FileProviderLogMock())
        }

        func exists(_ suffix: String = "") -> Bool {
            FileManager.default.fileExists(atPath: realmURL.path + suffix)
        }
    }

    static func makeFixture() -> Fixture {
        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("RealmStoreImporterTests-\(UUID().uuidString)", isDirectory: true)
        try! FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        return Fixture(directory: directory, domain: NSFileProviderDomainIdentifier("legacy-\(UUID().uuidString)"))
    }

    static let preciseSyncTime = Date(timeIntervalSinceReferenceDate: 790_000_000.123_456)

    /// Write a Realm file at the last Realm schema with one row in every table.
    static func writeLegacyStore(at fixture: Fixture, includeRows: Bool = true) throws {
        try autoreleasepool {
            let realm = try Realm(configuration: RealmStoreImporter.legacyConfiguration(fileURL: fixture.realmURL))
            defer { realm.invalidate() }

            guard includeRows else {
                return
            }

            try realm.write {
                var live = DatabaseTestSuites.makeFile(ocId: "live", fileName: "re\u{0301}sume\u{0301}.pdf", serverUrl: account.davFilesUrl + "/Docs")
                live.syncTime = preciseSyncTime
                live.tags = ["a", "b"]
                live.shareType = [0, 3]
                live.chunkUploadId = "upload-1"
                live.lockTime = Date(timeIntervalSinceReferenceDate: 790_000_001.5)
                realm.add(LegacyRealmItemMetadata(value: live))

                var gone = DatabaseTestSuites.makeFile(ocId: "gone", fileName: "gone.txt")
                gone.deleted = true
                realm.add(LegacyRealmItemMetadata(value: gone))

                realm.add(LegacyRealmExcludedFromSyncItem(ocId: "excluded"))
                realm.add(LegacyRealmRemoteFileChunk(RemoteFileChunk(fileName: "2", size: 20, remoteChunkStoreFolderName: "upload-1")))
                realm.add(LegacyRealmRemoteFileChunk(RemoteFileChunk(fileName: "1", size: 10, remoteChunkStoreFolderName: "upload-1")))
                // Realm never had a key for chunks, so a duplicate pair can exist on disk.
                realm.add(LegacyRealmRemoteFileChunk(RemoteFileChunk(fileName: "1", size: 10, remoteChunkStoreFolderName: "upload-1")))
                realm.add(LegacyRealmPendingChunkUploadCleanup(uploadIdentifier: "cleanup-1"))
                realm.add(LegacyRealmChangeDeliverySession(sessionId: "session", containerKey: "container", currentAnchorKey: "anchor", finalAnchorRawValue: Data([7]), incomplete: true, hardRemoveDeleted: true))
                for sequence in [2, 0, 1] {
                    realm.add(LegacyRealmChangeDeliveryItem(sessionId: "session", sequence: sequence, metadataData: Data([UInt8(sequence)]), deleted: sequence == 2))
                }
            }
        }
    }

    @Test func legacyClassesKeepTheirOnDiskNames() {
        #expect(LegacyRealmItemMetadata.className() == "RealmItemMetadata")
        #expect(LegacyRealmExcludedFromSyncItem.className() == "RealmExcludedFromSyncItem")
        #expect(LegacyRealmRemoteFileChunk.className() == "RemoteFileChunk")
        #expect(LegacyRealmPendingChunkUploadCleanup.className() == "RealmPendingChunkUploadCleanup")
        #expect(LegacyRealmChangeDeliverySession.className() == "RealmChangeDeliverySession")
        #expect(LegacyRealmChangeDeliveryItem.className() == "RealmChangeDeliveryItem")
    }

    @Test func everyTableIsImportedAndTheRealmFilesAreRemoved() throws {
        let fixture = Self.makeFixture()
        try Self.writeLegacyStore(at: fixture)
        #expect(fixture.exists(".lock"))

        let manager = fixture.openManager()

        let live = try #require(manager.itemMetadata(ocId: "live"))
        #expect(live.fileName == "re\u{0301}sume\u{0301}.pdf")
        #expect(live.syncTime == Self.preciseSyncTime)
        #expect(live.lockTime == Date(timeIntervalSinceReferenceDate: 790_000_001.5))
        #expect(live.tags == ["a", "b"])
        #expect(live.shareType == [0, 3])
        #expect(manager.normalizedLocationForTesting(ocId: "live")?.fileName == "résumé.pdf".precomposedStringWithCanonicalMapping)
        #expect(manager.itemMetadata(ocId: "gone")?.deleted == true)
        #expect(manager.isItemExcludedFromSync(ocId: "excluded"))
        #expect(manager.remoteFileChunks(uploadId: "upload-1").map(\.fileName) == ["1", "2"])
        #expect(manager.pendingChunkUploadCleanupIdentifiers() == ["cleanup-1"])
        let session = try #require(manager.changeDeliverySession(sessionId: "session"))
        #expect(session.containerKey == "container")
        #expect(session.incomplete)
        #expect(session.hardRemoveDeleted)
        #expect(session.finalAnchorRawValue == Data([7]))
        #expect(manager.changeDeliveryItems(sessionId: "session", fromSequence: 0, limit: 10).map(\.sequence) == [0, 1, 2])
        #expect(manager.changeDeliveryItems(sessionId: "session", fromSequence: 2, limit: 10).first?.deleted == true)

        #expect(FileManager.default.fileExists(atPath: fixture.sqliteURL.path))
        #expect(fixture.exists() == false)
        #expect(fixture.exists(".lock") == false)
        #expect(fixture.exists(".management") == false)
        #expect(fixture.exists(".note") == false)
        #expect(FileManager.default.fileExists(atPath: fixture.sqliteURL.path + RealmStoreImporter.stagingSuffix) == false)
    }

    @Test func anOlderSchemaIsUpgradedByRealmBeforeTheCopy() throws {
        let fixture = Self.makeFixture()
        let nfdServerUrl = "https://example.com/pre\u{0302}t"
        let nfdFileName = "pre\u{0302}t.pdf"

        try autoreleasepool {
            let oldConfiguration = Realm.Configuration(
                fileURL: fixture.realmURL,
                schemaVersion: LegacyRealmSchemaVersion.addedIsLockFileOfLocalOriginToRealmItemMetadata.rawValue,
                objectTypes: [LegacyRealmItemMetadata.self, LegacyRealmRemoteFileChunk.self]
            )
            let oldRealm = try Realm(configuration: oldConfiguration)
            defer { oldRealm.invalidate() }
            try oldRealm.write {
                let metadata = LegacyRealmItemMetadata()
                metadata.ocId = "migration-item"
                metadata.account = Self.account.ncKitAccount
                metadata.serverUrl = nfdServerUrl
                metadata.fileName = nfdFileName
                oldRealm.add(metadata)
            }
        }

        let manager = fixture.openManager()

        let migrated = try #require(manager.itemMetadata(ocId: "migration-item"))
        #expect(migrated.serverUrl == nfdServerUrl)
        let keys = try #require(manager.normalizedLocationForTesting(ocId: "migration-item"))
        #expect(keys.serverUrl == nfdServerUrl.precomposedStringWithCanonicalMapping)
        #expect(keys.fileName == nfdFileName.precomposedStringWithCanonicalMapping)
    }

    @Test func aRealmFileReplacesAnExistingDatabase() throws {
        let fixture = Self.makeFixture()
        let earlier = fixture.openManager()
        earlier.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "stale", fileName: "stale.txt"))
        #expect(earlier.itemMetadata(ocId: "stale") != nil)

        // An older build ran afterwards and left a Realm database behind: its content is the newer truth.
        try Self.writeLegacyStore(at: fixture)
        let manager = fixture.openManager()

        #expect(manager.itemMetadata(ocId: "stale") == nil)
        #expect(manager.itemMetadata(ocId: "live") != nil)
        #expect(fixture.exists() == false)
    }

    @Test func anUnreadableRealmFileIsSetAsideAndTheDatabaseStartsEmpty() throws {
        let fixture = Self.makeFixture()
        try Data((0 ..< 4096).map { _ in UInt8.random(in: 0 ... 255) }).write(to: fixture.realmURL)

        let manager = fixture.openManager()

        #expect(fixture.exists() == false)
        #expect(fixture.exists(RealmStoreImporter.failedSuffix))
        #expect(FileManager.default.fileExists(atPath: fixture.sqliteURL.path + RealmStoreImporter.stagingSuffix) == false)
        #expect(manager.allItemMetadatasForTesting().isEmpty)
        manager.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "fresh", fileName: "fresh.txt"))
        #expect(manager.itemMetadata(ocId: "fresh") != nil)
    }

    @Test func withoutARealmFileNothingIsImported() {
        let fixture = Self.makeFixture()

        #expect(RealmStoreImporter.importIfNeeded(realmURL: fixture.realmURL, sqliteURL: fixture.sqliteURL, logger: FileProviderLogger(category: "test", log: FileProviderLogMock())) == .notNeeded)

        let manager = fixture.openManager()
        #expect(manager.allItemMetadatasForTesting().isEmpty)
        #expect(FileManager.default.fileExists(atPath: fixture.sqliteURL.path))
    }

    @Test func aSecondOpenDoesNotImportAgain() throws {
        let fixture = Self.makeFixture()
        try Self.writeLegacyStore(at: fixture)
        _ = fixture.openManager()

        let manager = fixture.openManager()
        manager.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "added-later", fileName: "later.txt"))

        #expect(fixture.openManager().itemMetadata(ocId: "added-later") != nil)
        #expect(fixture.openManager().itemMetadata(ocId: "live") != nil)
    }
}
