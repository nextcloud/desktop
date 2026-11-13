//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import TestInterface
import Testing

///
/// Coverage for the one-time import of a Realm database written before the switch to SQLite.
///
/// Serialized because Realm keeps process-wide caches per file.
///
@Suite("Realm store import", .serialized)
struct RealmStoreImporterTests {
    enum MissingAccountDetails: CaseIterable {
        case accountKey
        case davRoot
        case accountKeyAndDAVRoot
    }

    enum DAVReadMode: CaseIterable {
        case item
        case completeListing
        case pagedListing
    }

    struct Fixture {
        let directory: URL
        let domain: NSFileProviderDomainIdentifier
        var realmURL: URL {
            directory.appendingPathComponent(domain.rawValue).appendingPathExtension("realm")
        }

        var sqliteURL: URL {
            directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension)
        }

        func openManager(account: Account = RealmStoreImporterTests.account) throws -> FilesDatabaseManager {
            try FilesDatabaseManager(
                account: account,
                databaseDirectory: directory,
                fileProviderDomainIdentifier: domain,
                log: FileProviderLogMock(),
                defaults: DatabaseTestSuites.defaults
            )
        }

        func savePreviouslyConfiguredAccount(_ account: Account) {
            var defaults = FileProviderDomainDefaults(identifier: domain, log: FileProviderLogMock(), defaults: DatabaseTestSuites.defaults)
            defaults.serverUrl = account.serverUrl
            defaults.user = account.username
            defaults.userId = account.id
        }

        func writeItems(_ items: [SendableItemMetadata]) throws {
            try autoreleasepool {
                let realm = try Realm(configuration: RealmStoreImporter.legacyConfiguration(fileURL: realmURL))
                defer { realm.invalidate() }
                try realm.write {
                    for item in items {
                        realm.add(LegacyRealmItemMetadata(value: item))
                    }
                }
            }
        }

        func exists(_ suffix: String = "") -> Bool {
            FileManager.default.fileExists(atPath: realmURL.path + suffix)
        }
    }

    static let account = DatabaseTestSuites.account

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
                live.downloaded = true
                live.keepDownloaded = true
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

    @Test func aFullDiskAndAHeldFileAreRecoverableOpenFailures() {
        #expect(RealmStoreImporter.isRecoverable(Realm.Error(.outOfDiskSpace)))
        #expect(RealmStoreImporter.isRecoverable(Realm.Error(.incompatibleSession)))
        #expect(RealmStoreImporter.isRecoverable(Realm.Error(.unsupportedFileFormatVersion)) == false)
        #expect(RealmStoreImporter.isRecoverable(NSError(domain: "test", code: 1)) == false)
    }

    @Test func everyTableIsImportedAndTheRealmFilesAreRemoved() throws {
        let fixture = Self.makeFixture()
        try Self.writeLegacyStore(at: fixture)
        #expect(fixture.exists(".lock"))

        let manager = try fixture.openManager()

        let live = try #require(manager.itemMetadata(ocId: "live"))
        #expect(live.fileName == "re\u{0301}sume\u{0301}.pdf")
        #expect(live.syncTime == Self.preciseSyncTime)
        #expect(live.lockTime == Date(timeIntervalSinceReferenceDate: 790_000_001.5))
        #expect(live.tags == ["a", "b"])
        #expect(live.shareType == [0, 3])
        #expect(manager.normalizedLocationForTesting(ocId: "live")?.fileName == "résumé.pdf".precomposedStringWithCanonicalMapping)
        #expect(manager.itemMetadata(ocId: "gone")?.deleted == true)
        #expect(manager.isItemExcludedFromSync(ocId: "excluded"))
        #expect(manager.remoteFileChunks(uploadId: "upload-1")?.map(\.fileName) == ["1", "2"])
        #expect(manager.pendingChunkUploadCleanupIdentifiers() == ["cleanup-1"])
        let session = try #require(try manager.changeDeliverySession(sessionId: "session"))
        #expect(session.containerKey == "container")
        #expect(session.incomplete)
        #expect(session.hardRemoveDeleted)
        #expect(session.finalAnchorRawValue == Data([7]))
        #expect(manager.changeDeliveryItems(sessionId: "session", fromSequence: 0, limit: 10)?.map(\.sequence) == [0, 1, 2])
        #expect(manager.changeDeliveryItems(sessionId: "session", fromSequence: 2, limit: 10)?.first?.deleted == true)

        #expect(FileManager.default.fileExists(atPath: fixture.sqliteURL.path))
        #expect(fixture.exists() == false)
        #expect(fixture.exists(".lock") == false)
        #expect(fixture.exists(".management") == false)
        #expect(fixture.exists(".note") == false)
        #expect(FileManager.default.fileExists(atPath: fixture.sqliteURL.path + RealmStoreImporter.stagingSuffix) == false)
    }

    @Test func importedItemsUseTheCurrentDAVBase() throws {
        let fixture = Self.makeFixture()
        defer { try? FileManager.default.removeItem(at: fixture.directory) }
        try Self.writeLegacyStore(at: fixture)
        let currentAccount = Account(
            user: "new-login",
            id: "new-id",
            serverUrl: "https://new.example.com/cloud",
            password: "password"
        )

        let foreignAccount = Account(
            user: "unrelated",
            id: "unrelated-id",
            serverUrl: "https://unrelated.example.com/cloud",
            password: "password"
        )
        var foreignFile = DatabaseTestSuites.makeFile(
            ocId: "foreign",
            fileName: "foreign.pdf",
            serverUrl: foreignAccount.davFilesUrl + "/Docs",
            account: foreignAccount
        )
        foreignFile.downloaded = true
        try fixture.writeItems([foreignFile])
        fixture.savePreviouslyConfiguredAccount(Self.account)

        let manager = try fixture.openManager(account: currentAccount)

        let live = try #require(manager.itemMetadata(ocId: "live"))
        let stored = try #require(try manager.storedParentLocationKeysForTesting(ocId: "live"))
        #expect(stored.davRoot == .files)
        #expect(stored.parentPath == "/Docs")
        #expect(live.serverUrl == currentAccount.davFilesUrl + "/Docs")
        #expect(live.account == currentAccount.ncKitAccount)
        #expect(live.downloaded)
        #expect(live.keepDownloaded)
        #expect(live.syncTime == Self.preciseSyncTime)
        #expect(manager.itemMetadata(ocId: foreignFile.ocId) == nil)
        #expect(try manager.storedParentLocationKeysForTesting(ocId: foreignFile.ocId) == nil)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: currentAccount.davFilesUrl + "/Docs/" + foreignFile.fileName) == nil)
        #expect(manager.materialisedItemMetadatas()?.contains { $0.ocId == foreignFile.ocId } == false)
    }

    @Test(arguments: [false, true], [false, true])
    func importUpdatesItemsFromPreviousServerWhenSettingsAlreadyUseCurrentServer(
        hasCurrentServerRows: Bool,
        backendUserIDChanged: Bool
    ) throws {
        let fixture = Self.makeFixture()
        defer { try? FileManager.default.removeItem(at: fixture.directory) }
        try Self.writeLegacyStore(at: fixture)

        let currentAccount = Account(
            user: Self.account.username,
            id: backendUserIDChanged ? "new-backend-id" : Self.account.id,
            serverUrl: "https://new.example.com/cloud",
            password: Self.account.password
        )
        fixture.savePreviouslyConfiguredAccount(currentAccount)
        let domainDefaults = FileProviderDomainDefaults(
            identifier: fixture.domain,
            log: FileProviderLogMock(),
            defaults: DatabaseTestSuites.defaults
        )

        var pending = DatabaseTestSuites.makeFile(
            ocId: "pending-edit",
            fileName: "draft.txt",
            serverUrl: Self.account.davFilesUrl + "/Docs",
            account: Self.account
        )
        pending.status = Status.uploadError.rawValue
        pending.uploaded = false
        pending.downloaded = true
        pending.keepDownloaded = true
        pending.chunkUploadId = "pending-upload"
        pending.lockToken = "old-lock"
        var alreadyRefreshed = DatabaseTestSuites.makeFile(
            ocId: "already-refreshed",
            fileName: "Listed before the upgrade.txt",
            serverUrl: currentAccount.davFilesUrl + "/Docs",
            account: currentAccount
        )
        alreadyRefreshed.uploaded = true
        alreadyRefreshed.lockToken = "current-lock"
        try autoreleasepool {
            let realm = try Realm(configuration: RealmStoreImporter.legacyConfiguration(fileURL: fixture.realmURL))
            defer { realm.invalidate() }
            try realm.write {
                realm.add(LegacyRealmItemMetadata(value: pending))
                if hasCurrentServerRows {
                    realm.add(LegacyRealmItemMetadata(value: alreadyRefreshed))
                }
            }
        }

        let outcome = RealmStoreImporter.importIfNeeded(
            realmURL: fixture.realmURL,
            sqliteURL: fixture.sqliteURL,
            account: currentAccount,
            previouslyConfiguredAccount: domainDefaults.configuredDAVAccountIdentity,
            logger: FileProviderLogger(category: "RealmStoreImporterTests", log: FileProviderLogMock())
        )
        guard case .imported = outcome else {
            Issue.record("Expected the Realm database to be imported.")
            return
        }
        #expect(fixture.exists() == false)

        // Inspect the SQLite file before opening the manager, which can also save the current DAV account.
        let database = try DatabaseQueue(path: fixture.sqliteURL.path)
        let identity = try database.read { db in
            let published = try #require(try Row.fetchOne(
                db,
                sql: "SELECT davRoot, parentPath FROM itemMetadata WHERE ocId = ?",
                arguments: ["pending-edit"]
            ))
            #expect(published["davRoot"] as String? == "files")
            #expect(published["parentPath"] as String? == "/Docs")
            let savedDAVAccountFields = try #require(try Data.fetchOne(db, sql: "SELECT accountBinding FROM storeIdentity"))
            #expect(try JSONDecoder().decode([String].self, from: savedDAVAccountFields) == currentAccount.davAccountIdentity.serializedFields)
            return try #require(try StoreIdentityRecord.fetchOne(db)).identity
        }

        let manager = try fixture.openManager(account: currentAccount)
        #expect(manager.storeIdentity == identity)
        let live = try #require(manager.itemMetadata(ocId: "live"))
        #expect(live.serverUrl == currentAccount.davFilesUrl + "/Docs")
        #expect(live.downloaded)
        #expect(live.keepDownloaded)
        let saved = try #require(manager.itemMetadata(ocId: pending.ocId))
        #expect(saved.hasUnfinishedUpload)
        #expect(saved.chunkUploadId == pending.chunkUploadId)
        #expect(saved.downloaded)
        #expect(saved.keepDownloaded)
        #expect(saved.lockToken == nil)
        if hasCurrentServerRows {
            let refreshed = try #require(manager.itemMetadata(ocId: alreadyRefreshed.ocId))
            #expect(refreshed.remotePath() == alreadyRefreshed.remotePath())
            #expect(refreshed.lockToken == alreadyRefreshed.lockToken)
        }
    }

    @Test(arguments: [false, true])
    func importedItemsAppearInTheWorkingSetBeforeAnyListing(settingsAlreadyCurrent: Bool) async throws {
        let fixture = Self.makeFixture()
        defer { try? FileManager.default.removeItem(at: fixture.directory) }
        let previousAccount = Account(
            user: Self.account.username,
            id: Self.account.id,
            serverUrl: "https://old.example.com",
            password: Self.account.password
        )
        let earlierAccount = Account(
            user: Self.account.username,
            id: "earlier-backend-id",
            serverUrl: "https://earlier.example.com/cloud",
            password: Self.account.password
        )
        let foreignAccount = Account(
            user: "unrelated-login",
            id: "unrelated-backend-id",
            serverUrl: previousAccount.serverUrl,
            password: Self.account.password
        )
        var folder = DatabaseTestSuites.makeFile(
            ocId: "cached-folder",
            fileName: "re\u{0301}sume\u{0301}s",
            account: previousAccount
        )
        folder.directory = true
        folder.visitedDirectory = true
        var child = DatabaseTestSuites.makeFile(
            ocId: "downloaded-file",
            fileName: "report.pdf",
            serverUrl: earlierAccount.davFilesUrl + "/résumés",
            account: earlierAccount
        )
        child.account = ""
        child.downloaded = true
        child.keepDownloaded = true
        child.status = Status.uploading.rawValue
        child.chunkUploadId = "pending-upload"
        child.session = "interrupted-upload"
        child.lockToken = "earlier-lock"
        var foreignFile = DatabaseTestSuites.makeFile(
            ocId: "foreign-file",
            fileName: "foreign.pdf",
            account: foreignAccount
        )
        foreignFile.downloaded = true
        var invalidFile = DatabaseTestSuites.makeFile(
            ocId: "invalid-file",
            fileName: "invalid.pdf",
            serverUrl: "https://unrelated.example.com" + Account.webDavFilesUrlSuffix + previousAccount.id,
            account: previousAccount
        )
        invalidFile.downloaded = true
        let sessionId = "earlier-server-changes"
        let queuedDeletion = try JSONEncoder().encode(child)
        try autoreleasepool {
            let realm = try Realm(configuration: RealmStoreImporter.legacyConfiguration(fileURL: fixture.realmURL))
            defer { realm.invalidate() }
            try realm.write {
                for item in [folder, child, foreignFile, invalidFile] {
                    realm.add(LegacyRealmItemMetadata(value: item))
                }
                realm.add(LegacyRealmChangeDeliverySession(
                    sessionId: sessionId,
                    containerKey: folder.ocId,
                    currentAnchorKey: "old-anchor",
                    finalAnchorRawValue: Data([7]),
                    incomplete: true,
                    hardRemoveDeleted: true
                ))
                realm.add(LegacyRealmChangeDeliveryItem(
                    sessionId: sessionId,
                    sequence: 0,
                    metadataData: queuedDeletion,
                    deleted: true
                ))
            }
        }
        fixture.savePreviouslyConfiguredAccount(settingsAlreadyCurrent ? Self.account : previousAccount)

        let manager = try fixture.openManager()
        #expect(manager.itemMetadata(ocId: folder.ocId)?.serverUrl == Self.account.davFilesUrl)
        let savedChild = try #require(manager.itemMetadata(ocId: child.ocId))
        #expect(savedChild.serverUrl == Self.account.davFilesUrl + "/résumés")
        #expect(savedChild.account == Self.account.ncKitAccount)
        #expect(savedChild.userId == Self.account.id)
        #expect(savedChild.downloaded)
        #expect(savedChild.keepDownloaded)
        #expect(savedChild.hasUnfinishedUpload)
        #expect(savedChild.chunkUploadId == child.chunkUploadId)
        #expect(savedChild.session == "")
        #expect(savedChild.lockToken == nil)
        #expect(manager.itemMetadata(ocId: foreignFile.ocId) == nil)
        #expect(manager.itemMetadata(ocId: invalidFile.ocId) == nil)
        #expect(try manager.changeDeliverySession(sessionId: sessionId) == nil)
        #expect(manager.changeDeliveryItems(sessionId: sessionId, fromSequence: 0, limit: 1)?.isEmpty == true)
        let remote = MockRemoteInterface(account: Self.account)
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            remoteInterface: remote,
            dbManager: manager,
            log: FileProviderLogMock()
        )
        let snapshot = MockEnumerationObserver(enumerator: enumerator)
        try await snapshot.enumerateItems()
        #expect(Set(snapshot.items.map(\.itemIdentifier.rawValue)) == [folder.ocId, child.ocId])
        let retainedChild = try #require(snapshot.items.first { $0.itemIdentifier.rawValue == child.ocId })
        #expect(retainedChild.parentItemIdentifier.rawValue == folder.ocId)
        #expect(remote.readOperationCount == 0)
    }

    @Test(arguments: MissingAccountDetails.allCases)
    func importedRowWithMissingAccountDetailsUsesADAVRelativeParent(missing: MissingAccountDetails) throws {
        let fixture = Self.makeFixture()
        defer { try? FileManager.default.removeItem(at: fixture.directory) }
        var file = DatabaseTestSuites.makeFile(
            ocId: "legacy-report",
            fileName: "report.pdf",
            serverUrl: Self.account.davFilesUrl + "/Docs",
            account: Self.account
        )
        if missing != .davRoot {
            file.account = ""
        }
        if missing != .accountKey {
            file.urlBase = ""
            file.userId = ""
        }
        file.keepDownloaded = true

        try autoreleasepool {
            let realm = try Realm(configuration: RealmStoreImporter.legacyConfiguration(fileURL: fixture.realmURL))
            defer { realm.invalidate() }
            try realm.write { realm.add(LegacyRealmItemMetadata(value: file)) }
        }

        let manager = try fixture.openManager()
        let stored = try #require(try manager.storedParentLocationKeysForTesting(ocId: file.ocId))
        let restored = try #require(manager.itemMetadata(locatedAtRemoteUrl: file.remotePath()))
        #expect(stored.davRoot == .files)
        #expect(stored.parentPath == "/Docs")
        #expect(restored.ocId == file.ocId)
        #expect(restored.account == Self.account.ncKitAccount)
        #expect(restored.userId == Self.account.id)
        #expect(restored.keepDownloaded)
    }

    @Test(arguments: [false, true], DAVReadMode.allCases)
    func davReadPreservesLocalStateOfImportedLegacyItem(isDirectory: Bool, readMode: DAVReadMode) async throws {
        let fixture = Self.makeFixture()
        defer { try? FileManager.default.removeItem(at: fixture.directory) }
        let account = Self.account
        let previousAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://previous.example.com",
            password: account.password
        )
        var item = DatabaseTestSuites.makeFile(
            ocId: "cached-item",
            fileName: isDirectory ? "Reports" : "Report.txt",
            serverUrl: previousAccount.davFilesUrl,
            account: previousAccount
        )
        item.directory = isDirectory
        item.downloaded = true
        item.keepDownloaded = true
        item.visitedDirectory = isDirectory
        item.uploaded = true
        item.etag = "cached-etag"
        item.fileProviderContentVersion = "cached-version"
        item.lockToken = "previous-lock"
        try autoreleasepool {
            let realm = try Realm(configuration: RealmStoreImporter.legacyConfiguration(fileURL: fixture.realmURL))
            defer { realm.invalidate() }
            try realm.write { realm.add(LegacyRealmItemMetadata(value: item)) }
        }
        fixture.savePreviouslyConfiguredAccount(account)
        let manager = try fixture.openManager()
        let imported = try #require(manager.itemMetadata(ocId: item.ocId))
        #expect(imported.serverUrl == account.davFilesUrl)
        #expect(imported.keepDownloaded)
        #expect(imported.downloaded)
        #expect(imported.lockToken == nil)

        let root = MockRemoteItem.rootItem(account: account)
        let listedItem = MockRemoteItem(
            identifier: item.ocId,
            versionIdentifier: item.etag,
            name: item.fileName,
            remotePath: account.davFilesUrl + "/" + item.fileName,
            directory: isDirectory,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        root.children = [listedItem]
        listedItem.parent = root
        let remote = MockRemoteInterface(account: account, rootItem: root)
        remote.pagination = readMode == .pagedListing
        let listing = await Enumerator.readServerUrl(
            readMode == .item ? listedItem.remotePath : account.davFilesUrl,
            pageSettings: readMode == .pagedListing ? (page: nil, index: 0, size: 100) : nil,
            remoteInterface: remote,
            dbManager: manager,
            depth: readMode == .item ? .target : .targetAndDirectChildren,
            log: FileProviderLogMock()
        )

        #expect(listing.error == nil)
        let reported = try #require(listing.metadatas?.first { $0.ocId == item.ocId })
        let saved = try #require(manager.itemMetadata(ocId: item.ocId))
        #expect(reported.keepDownloaded)
        #expect(reported.downloaded)
        #expect(reported.visitedDirectory == isDirectory)
        #expect(reported.fileProviderContentVersion == item.fileProviderContentVersion)
        #expect(reported.lockToken == nil)
        #expect(saved == reported)
        #expect(saved.remotePath() == listedItem.remotePath)
    }

    @Test func importedSDKWebrootAliasUsesADAVRelativeParent() throws {
        let fixture = Self.makeFixture()
        defer { try? FileManager.default.removeItem(at: fixture.directory) }
        let account = Account(
            user: "alice",
            id: "alice",
            serverUrl: "https://example.com/caf%C3%A9",
            password: "password"
        )
        let file = DatabaseTestSuites.makeFile(
            ocId: "legacy-report",
            fileName: "report.pdf",
            serverUrl: "https://example.com/cafe\u{0301}" + Account.webDavFilesUrlSuffix + account.id + "/Docs",
            account: account
        )
        try autoreleasepool {
            let realm = try Realm(configuration: RealmStoreImporter.legacyConfiguration(fileURL: fixture.realmURL))
            defer { realm.invalidate() }
            try realm.write { realm.add(LegacyRealmItemMetadata(value: file)) }
        }

        let manager = try fixture.openManager(account: account)
        let stored = try #require(try manager.storedParentLocationKeysForTesting(ocId: file.ocId))
        let canonicalFileURL = account.davFilesUrl + "/Docs/" + file.fileName
        #expect(stored.davRoot == .files)
        #expect(stored.parentPath == "/Docs")
        #expect(manager.itemMetadata(locatedAtRemoteUrl: canonicalFileURL)?.ocId == file.ocId)
    }

    @Test func anOlderSchemaIsUpgradedByRealmBeforeTheCopy() throws {
        let fixture = Self.makeFixture()
        let nfdServerUrl = Self.account.davFilesUrl + "/pre\u{0302}t"
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

        let manager = try fixture.openManager()

        let migrated = try #require(manager.itemMetadata(ocId: "migration-item"))
        #expect(migrated.serverUrl == nfdServerUrl)
        let keys = try #require(manager.normalizedLocationForTesting(ocId: "migration-item"))
        #expect(keys.serverUrl == "/prêt")
        #expect(keys.fileName == nfdFileName.precomposedStringWithCanonicalMapping)
    }

    @Test func importedTrashItemCanBeRestoredBeforeAnyListing() async throws {
        let fixture = Self.makeFixture()
        defer { try? FileManager.default.removeItem(at: fixture.directory) }
        let account = Self.account
        let previousAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://old.example.com",
            password: account.password
        )
        let foreignAccount = Account(
            user: "different-login",
            id: "different-id",
            serverUrl: previousAccount.serverUrl,
            password: account.password
        )
        let root = MockRemoteItem.rootItem(account: account)
        let trash = MockRemoteItem.rootTrashItem(account: account)
        let remoteFile = MockRemoteItem(
            identifier: "trashed-report",
            name: "report.pdf.d1760000000",
            remotePath: account.trashUrl + "/report.pdf.d1760000000",
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl,
            trashbinOriginalLocation: "report.pdf"
        )
        trash.children = [remoteFile]
        remoteFile.parent = trash
        let remote = MockRemoteInterface(account: account, rootItem: root, rootTrashItem: trash)
        var cachedTrashEntry = remoteFile.toNKTrash().toItemMetadata(account: previousAccount)
        cachedTrashEntry.serverUrl = previousAccount.trashUrl + "/"
        cachedTrashEntry.downloaded = true
        var foreignTrashEntry = remoteFile.toNKTrash().toItemMetadata(account: foreignAccount)
        foreignTrashEntry.ocId = "foreign-trash"
        let earlierFile = DatabaseTestSuites.makeFileFromEarlierServerURL(account: account)
        try autoreleasepool {
            let realm = try Realm(configuration: RealmStoreImporter.legacyConfiguration(fileURL: fixture.realmURL))
            defer { realm.invalidate() }
            try realm.write {
                for item in [cachedTrashEntry, foreignTrashEntry, earlierFile] {
                    realm.add(LegacyRealmItemMetadata(value: item))
                }
            }
        }
        fixture.savePreviouslyConfiguredAccount(account)

        let manager = try fixture.openManager(account: account)
        let log = FileProviderLogMock()
        let item = try await Item.lookupItemAndFindParentIdentifier(
            identifier: .init(cachedTrashEntry.ocId),
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            log: log
        )
        #expect(remote.readOperationCount == 0)
        #expect(item.parentItemIdentifier == .trashContainer)
        #expect(manager.parentItemIdentifierFromMetadata(item.metadata) == .trashContainer)
        #expect(item.metadata.serverUrl == account.trashUrl + "/")
        #expect(item.metadata.downloaded)
        #expect(manager.itemMetadata(ocId: foreignTrashEntry.ocId) == nil)
        #expect(manager.itemMetadata(ocId: earlierFile.ocId)?.remotePath() == account.davFilesUrl + "/" + earlierFile.fileName)

        let (restored, error) = await Item.restoreFromTrash(
            item,
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            domain: nil,
            log: log
        )
        #expect(error == nil)
        #expect(restored.itemIdentifier.rawValue == cachedTrashEntry.ocId)
        #expect(manager.itemMetadata(ocId: cachedTrashEntry.ocId)?.remotePath() == account.davFilesUrl + "/report.pdf")
    }

    @Test func ordinaryListingReconcilesServerRenameAfterImport() async throws {
        let fixture = Self.makeFixture()
        defer { try? FileManager.default.removeItem(at: fixture.directory) }
        let account = Self.account
        let previousAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://old.example.com",
            password: account.password
        )
        let teamURL = account.davFilesUrl + "/Team"
        var original = DatabaseTestSuites.makeFile(
            ocId: "original-report",
            fileName: "Report.txt",
            serverUrl: previousAccount.davFilesUrl + "/Team",
            account: previousAccount
        )
        original.downloaded = true
        original.keepDownloaded = true
        original.uploaded = true
        let replacementID = "replacement-report"
        let root = MockRemoteItem.rootItem(account: account)
        let team = MockRemoteItem(
            identifier: "team-folder",
            name: "Team",
            remotePath: teamURL,
            directory: true,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        root.children = [team]
        team.parent = root
        var directory = DatabaseTestSuites.makeFile(ocId: team.identifier, fileName: team.name, account: previousAccount)
        directory.directory = true
        directory.uploaded = true
        try autoreleasepool {
            let realm = try Realm(configuration: RealmStoreImporter.legacyConfiguration(fileURL: fixture.realmURL))
            defer { realm.invalidate() }
            try realm.write {
                for item in [original, directory] {
                    realm.add(LegacyRealmItemMetadata(value: item))
                }
            }
        }
        fixture.savePreviouslyConfiguredAccount(account)

        let manager = try fixture.openManager()
        let imported = try #require(manager.itemMetadata(ocId: original.ocId))
        #expect(imported.remotePath() == teamURL + "/" + original.fileName)
        #expect(imported.downloaded)
        #expect(imported.keepDownloaded)
        #expect(!imported.deleted)

        let currentNames = [(original.ocId, "Older report.txt"), (replacementID, original.fileName)]
        for (identifier, name) in currentNames {
            let item = MockRemoteItem(
                identifier: identifier,
                versionIdentifier: "file-version",
                name: name,
                remotePath: teamURL + "/" + name,
                account: account.ncKitAccount,
                username: account.username,
                userId: account.id,
                serverUrl: account.serverUrl
            )
            team.children.append(item)
            item.parent = team
        }
        let remote = MockRemoteInterface(account: account, rootItem: root)
        let listing = await Enumerator.readServerUrl(
            teamURL,
            remoteInterface: remote,
            dbManager: manager,
            depth: .targetAndDirectChildren,
            log: FileProviderLogMock()
        )

        #expect(listing.error == nil)
        #expect(listing.changes?.deleted.isEmpty == true)
        #expect(remote.readOperationCount == 1)
        let moved = try #require(manager.itemMetadata(ocId: original.ocId))
        #expect(moved.remotePath() == teamURL + "/Older report.txt")
        #expect(moved.downloaded)
        #expect(moved.keepDownloaded)
        #expect(manager.itemMetadata(ocId: replacementID)?.remotePath() == teamURL + "/" + original.fileName)
    }

    @Test func aRealmFileReplacesAnExistingDatabase() throws {
        let fixture = Self.makeFixture()
        let earlier = try fixture.openManager()
        earlier.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "stale", fileName: "stale.txt"))
        #expect(earlier.itemMetadata(ocId: "stale") != nil)

        // An older build ran afterwards and left a Realm database behind: its content is the newer truth.
        try Self.writeLegacyStore(at: fixture)
        let manager = try fixture.openManager()

        #expect(manager.itemMetadata(ocId: "stale") == nil)
        #expect(manager.itemMetadata(ocId: "live") != nil)
        #expect(manager.storeIdentity != earlier.storeIdentity)
        #expect(fixture.exists() == false)
    }

    @Test func anUnreadableRealmFileIsSetAsideAndTheDatabaseStartsEmpty() throws {
        let fixture = Self.makeFixture()
        try Data((0 ..< 4096).map { _ in UInt8.random(in: 0 ... 255) }).write(to: fixture.realmURL)

        let manager = try fixture.openManager()

        #expect(fixture.exists() == false)
        #expect(fixture.exists(RealmStoreImporter.failedSuffix))
        #expect(FileManager.default.fileExists(atPath: fixture.sqliteURL.path + RealmStoreImporter.stagingSuffix) == false)
        #expect(manager.allItemMetadatasForTesting().isEmpty)
        manager.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "fresh", fileName: "fresh.txt"))
        #expect(manager.itemMetadata(ocId: "fresh") != nil)
    }

    @Test func withoutARealmFileNothingIsImported() throws {
        let fixture = Self.makeFixture()

        let outcome = RealmStoreImporter.importIfNeeded(
            realmURL: fixture.realmURL,
            sqliteURL: fixture.sqliteURL,
            account: Self.account,
            previouslyConfiguredAccount: nil,
            logger: FileProviderLogger(category: "RealmStoreImporterTests", log: FileProviderLogMock())
        )
        #expect(outcome == .notNeeded)

        let manager = try fixture.openManager()
        #expect(manager.allItemMetadatasForTesting().isEmpty)
        #expect(FileManager.default.fileExists(atPath: fixture.sqliteURL.path))
    }

    @Test func aRecoverableFailureKeepsTheRealmFileAndFailsTheOpen() throws {
        let fixture = Self.makeFixture()
        try Self.writeLegacyStore(at: fixture)
        // A directory nothing can be created in: whichever step fails first, the file must survive for the next start.
        try FileManager.default.setAttributes([.posixPermissions: 0o555], ofItemAtPath: fixture.directory.path)
        defer { try? FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: fixture.directory.path) }

        #expect(throws: FilesDatabaseManager.OpenError.self) {
            try fixture.openManager()
        }

        #expect(fixture.exists())
        #expect(fixture.exists(RealmStoreImporter.failedSuffix) == false)
        #expect(FileManager.default.fileExists(atPath: fixture.sqliteURL.path) == false)

        try FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: fixture.directory.path)
        #expect(try fixture.openManager().itemMetadata(ocId: "live") != nil, "The next start imports the untouched file.")
    }

    @Test func aSecondOpenDoesNotImportAgain() throws {
        let fixture = Self.makeFixture()
        try Self.writeLegacyStore(at: fixture)
        _ = try fixture.openManager()

        let manager = try fixture.openManager()
        manager.addItemMetadata(DatabaseTestSuites.makeFile(ocId: "added-later", fileName: "later.txt"))

        #expect(try fixture.openManager().itemMetadata(ocId: "added-later") != nil)
        #expect(try fixture.openManager().itemMetadata(ocId: "live") != nil)
    }
}
