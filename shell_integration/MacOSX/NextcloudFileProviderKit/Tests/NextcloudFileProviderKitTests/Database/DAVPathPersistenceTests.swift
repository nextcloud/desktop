//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import GRDB
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import Testing
import TestInterface

@Suite("Persisted DAV paths")
struct DAVPathPersistenceTests {
    enum EnumerationScope: CaseIterable {
        case workingSet
        case teamFolder

        var identifier: NSFileProviderItemIdentifier {
            switch self {
                case .workingSet: .workingSet
                case .teamFolder: .init(DAVPathPersistenceTests.folderId)
            }
        }
    }

    enum DAVAccountChange: CaseIterable {
        case serverBaseURL
        case backendUserID

        var accountAfterChange: Account {
            let old = DAVPathPersistenceTests.oldAccount
            let current = DAVPathPersistenceTests.currentAccount
            return Account(
                user: old.username,
                id: self == .backendUserID ? current.id : old.id,
                serverUrl: self == .serverBaseURL ? current.serverUrl : old.serverUrl,
                password: old.password
            )
        }
    }

    enum QueuedParentFormat: CaseIterable {
        case relativeParentPath
        case equivalentResponseWebroot
        case absoluteParentURL
    }

    private static let oldAccount = Account(
        user: "old-login",
        id: "old-id",
        serverUrl: "https://old.example.com/cloud",
        password: "password"
    )
    private static let currentAccount = Account(
        user: "new-login",
        id: "new-id",
        serverUrl: "https://new.example.com/nextcloud",
        password: "password"
    )
    private static let previousHostAccount = Account(
        user: currentAccount.username,
        id: currentAccount.id,
        serverUrl: "https://previous.example.com/cloud",
        password: currentAccount.password
    )
    private static let folderId = "team-folder-id"
    private static let folderName = "Team Re\u{0301}ports"
    private static let fileId = "report-id"
    private static let fileName = "Report #1.txt"
    private static let remoteETag = "new-version"
    private static let storedETag = "old-version"

    private let directory: URL
    private let domain = NSFileProviderDomainIdentifier("dav-paths-\(UUID().uuidString)")
    private let log = FileProviderLogMock()

    init() throws {
        directory = FileManager.default.temporaryDirectory
            .appendingPathComponent(domain.rawValue, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
    }

    @Test func startupRebasesRowsForSavedPreviousAccountWithoutLosingLocalState() throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let foreign = Account(
            user: "foreign-login",
            id: "foreign-id",
            serverUrl: Self.oldAccount.serverUrl,
            password: "password"
        )
        var foreignFile = DatabaseTestSuites.makeFile(
            ocId: "foreign-file-id",
            fileName: "foreign-report.pdf",
            serverUrl: foreign.davFilesUrl + "/" + Self.folderName,
            account: foreign
        )
        foreignFile.downloaded = true
        var legacyFile = DatabaseTestSuites.makeFile(
            ocId: "legacy-file-id",
            fileName: "legacy-report.pdf",
            serverUrl: Self.oldAccount.davFilesUrl + "/" + Self.folderName,
            account: Self.oldAccount
        )
        legacyFile.urlBase = ""
        legacyFile.userId = ""
        legacyFile.downloaded = true
        let identity = try writeUnboundStoreWithMaterializedItems(additionalMetadata: [foreignFile, legacyFile])

        let manager = try openManager()
        let folder = try #require(manager.itemMetadata(ocId: Self.folderId))
        let file = try #require(manager.itemMetadata(ocId: Self.fileId))
        let parentUrl = Self.currentAccount.davFilesUrl + "/" + Self.folderName
        let storedFolder = try #require(try manager.storedParentLocationKeysForTesting(ocId: Self.folderId))
        let storedFile = try #require(try manager.storedParentLocationKeysForTesting(ocId: Self.fileId))

        #expect(manager.storeIdentity != identity)
        #expect(storedFolder.parentLocation == "files:")
        #expect(storedFile.parentLocation == "files:/" + Self.folderName)
        #expect(storedFile.normalizedParentLocation.utf8.elementsEqual(("files:/" + Self.folderName).precomposedStringWithCanonicalMapping.utf8))
        #expect(folder.serverUrl == Self.currentAccount.davFilesUrl)
        #expect(folder.visitedDirectory)
        #expect(file.serverUrl == parentUrl)
        #expect(file.davParentPathWithoutOrigin == Self.filesRootPathWithoutOrigin(for: Self.currentAccount) + "/" + Self.folderName + "/")
        #expect(file.account == Self.currentAccount.ncKitAccount)
        #expect(file.user == Self.currentAccount.username)
        #expect(file.userId == Self.currentAccount.id)
        #expect(file.urlBase == Self.currentAccount.serverUrl)
        #expect(file.downloaded)
        #expect(file.keepDownloaded)
        #expect(file.etag == Self.storedETag)
        #expect(!file.deleted)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: parentUrl + "/" + Self.fileName)?.ocId == Self.fileId)

        let migratedLegacyFile = try #require(manager.itemMetadata(locatedAtRemoteUrl: parentUrl + "/" + legacyFile.fileName))
        #expect(migratedLegacyFile.ocId == legacyFile.ocId)
        #expect(migratedLegacyFile.account == Self.currentAccount.ncKitAccount)
        #expect(migratedLegacyFile.downloaded)

        let retainedForeignFile = try #require(manager.itemMetadatas(account: foreign.ncKitAccount).first { $0.ocId == foreignFile.ocId })
        #expect(manager.itemMetadata(ocId: foreignFile.ocId) == nil)
        #expect(retainedForeignFile.remotePath() == foreignFile.remotePath())
        #expect(retainedForeignFile.account == foreign.ncKitAccount)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: parentUrl + "/" + foreignFile.fileName) == nil)
        #expect(manager.materialisedItemMetadatas()?.contains { $0.ocId == foreignFile.ocId } == false)
        let changes = try #require(manager.pendingWorkingSetChanges(since: .distantPast))
        #expect(changes.updated.contains { $0.ocId == foreignFile.ocId } == false)
        #expect(changes.deleted.contains { $0.ocId == foreignFile.ocId } == false)

        let reopened = try openManager()

        #expect(reopened.storeIdentity == manager.storeIdentity)
        #expect(reopened.itemMetadata(ocId: Self.fileId)?.remotePath() == file.remotePath())
        #expect(try reopened.storedParentLocationKeysForTesting(ocId: Self.fileId)?.parentLocation == storedFile.parentLocation)
    }

    @Test(arguments: [false, true])
    func movingLegacyMetadataIntoTheFilesRootStoresRelativeParents(movesDirectory: Bool) throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let account = Self.currentAccount
        let legacyRootURL = account.serverUrl + "/remote.php/webdav"
        var folder = DatabaseTestSuites.makeFile(
            ocId: Self.folderId,
            fileName: Self.folderName,
            serverUrl: legacyRootURL,
            account: account
        )
        folder.directory = true
        folder.uploaded = true
        folder.visitedDirectory = true
        folder.account = ""
        folder.urlBase = ""
        folder.userId = ""
        var file = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            serverUrl: folder.remotePath(),
            account: account
        )
        file.account = ""
        file.urlBase = ""
        file.userId = ""
        file.uploaded = true
        file.downloaded = true
        file.keepDownloaded = true
        _ = try FilesDatabaseManager.writeUnboundStoreForTesting(
            at: directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension),
            metadata: [folder, file]
        )
        let manager = try openManager()
        try #require(manager.itemMetadata(locatedAtRemoteUrl: file.remotePath())?.ocId == file.ocId)
        try #require(try manager.storedParentLocationKeysForTesting(ocId: file.ocId)?.parentLocation == file.serverUrl)

        let destinationFileParentURL: String
        let destinationFileParentPath: String
        if movesDirectory {
            let movedChildren = try #require(manager.renameDirectoryAndPropagateToChildren(
                ocId: folder.ocId,
                newServerUrl: account.davFilesUrl,
                newFileName: folder.fileName
            ))
            destinationFileParentURL = account.davFilesUrl + "/" + folder.fileName
            destinationFileParentPath = "files:/" + folder.fileName
            #expect(try manager.storedParentLocationKeysForTesting(ocId: folder.ocId)?.parentLocation == "files:")
            #expect(movedChildren.map(\.ocId) == [file.ocId])
            let movedFolder = try #require(manager.itemMetadata(locatedAtRemoteUrl: destinationFileParentURL))
            #expect(manager.immediateChildItems(directoryMetadata: movedFolder).map(\.ocId) == [file.ocId])
        } else {
            manager.renameItemMetadata(
                ocId: file.ocId,
                newServerUrl: account.davFilesUrl,
                newFileName: file.fileName
            )
            destinationFileParentURL = account.davFilesUrl
            destinationFileParentPath = "files:"
        }

        #expect(try manager.storedParentLocationKeysForTesting(ocId: file.ocId)?.parentLocation == destinationFileParentPath)
        let movedFile = try #require(manager.itemMetadata(locatedAtRemoteUrl: destinationFileParentURL + "/" + file.fileName))
        #expect(movedFile.ocId == file.ocId)
        #expect(movedFile.account == account.ncKitAccount)
        #expect(movedFile.userId == account.id)
        #expect(movedFile.downloaded)
        #expect(movedFile.keepDownloaded)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: file.remotePath()) == nil)
    }

    @Test(arguments: [false, true])
    func ambiguousPreviousHostsKeepOldItemWhenListingReturnsItsID(fileUploadFailed: Bool) async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let account = Self.currentAccount
        let previous = Self.previousHostAccount
        _ = try writeUnboundStoreWithMaterializedItems(
            oldAccount: previous,
            additionalMetadata: [DatabaseTestSuites.makeFileFromEarlierServerURL(account: account)],
            fileUploadFailed: fileUploadFailed
        )
        savePreviouslyConfiguredAccount(account)

        let manager = try openManager()
        let remote = makeRemoteWithTeamFolder()
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: .workingSet,
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            log: log
        )
        let observer = MockEnumerationObserver(enumerator: enumerator)
        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await observer.enumerateItems()
        }
        #expect(observer.items.isEmpty)
        #expect(remote.readOperationCount == 0)

        let listing = await listDirectory(
            account.davFilesUrl + "/" + Self.folderName,
            manager: manager,
            remote: remote
        )
        #expect(listing.error != nil)
        #expect(manager.itemMetadata(ocId: Self.fileId) == nil)
        let retained = try #require(manager.itemMetadatas(account: previous.ncKitAccount).first {
            $0.ocId == Self.fileId
        })
        #expect(retained.hasUnfinishedUpload == fileUploadFailed)
        #expect(retained.keepDownloaded)
        #expect(retained.downloaded)
        #expect(!retained.deleted)
    }

    @Test func startupFillsMissingAccountDetailsForMatchingDAVParents() throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let account = Self.currentAccount
        var report = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            serverUrl: account.davFilesUrl + "/Docs",
            account: account
        )
        report.urlBase = ""
        report.userId = ""
        report.downloaded = true
        report.keepDownloaded = true

        var trashed = DatabaseTestSuites.makeFile(
            ocId: "trashed",
            fileName: "trash.txt",
            serverUrl: account.trashUrl,
            account: account
        )
        trashed.account = ""
        trashed.user = ""
        trashed.userId = ""

        var missingAccountKey = DatabaseTestSuites.makeFile(
            ocId: "missing-account-key",
            fileName: "keyless.txt",
            serverUrl: report.serverUrl,
            account: account
        )
        missingAccountKey.account = ""
        missingAccountKey.downloaded = true

        var outsideRoot = report
        outsideRoot.ocId = "outside-root"
        outsideRoot.serverUrl = account.davFilesUrl + "-other/Docs"

        var differentAccount = report
        differentAccount.ocId = "different-account"
        let otherServer = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://other.example.com",
            password: "password"
        )
        differentAccount.account = otherServer.ncKitAccount
        differentAccount.serverUrl = otherServer.davFilesUrl + "/Docs"
        differentAccount.userId = otherServer.id

        _ = try FilesDatabaseManager.writeUnboundStoreForTesting(
            at: directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension),
            metadata: [report, trashed, missingAccountKey, outsideRoot, differentAccount]
        )

        let manager = try openManager()
        let storedReport = try #require(try manager.storedParentLocationKeysForTesting(ocId: report.ocId))
        let restoredReport = try #require(manager.itemMetadata(locatedAtRemoteUrl: report.remotePath()))
        #expect(storedReport.parentLocation == "files:/Docs")
        #expect(storedReport.normalizedParentLocation == "files:/Docs")
        #expect(restoredReport.ocId == report.ocId)
        #expect(restoredReport.urlBase == account.serverUrl)
        #expect(restoredReport.userId == account.id)
        #expect(restoredReport.downloaded)
        #expect(restoredReport.keepDownloaded)

        let storedTrash = try #require(try manager.storedParentLocationKeysForTesting(ocId: trashed.ocId))
        let restoredTrash = try #require(manager.itemMetadata(locatedAtRemoteUrl: trashed.remotePath()))
        #expect(storedTrash.parentLocation == "trash:")
        #expect(restoredTrash.ocId == trashed.ocId)
        #expect(restoredTrash.account == account.ncKitAccount)
        #expect(restoredTrash.user == account.username)
        #expect(restoredTrash.userId == account.id)

        #expect(manager.itemMetadata(ocId: missingAccountKey.ocId)?.account == account.ncKitAccount)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: missingAccountKey.remotePath())?.ocId == missingAccountKey.ocId)
        #expect(manager.materialisedItemMetadatas()?.contains { $0.ocId == missingAccountKey.ocId } == true)

        #expect(try manager.storedParentLocationKeysForTesting(ocId: outsideRoot.ocId)?.parentLocation == outsideRoot.serverUrl)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: outsideRoot.remotePath())?.ocId == outsideRoot.ocId)
        #expect(try manager.storedParentLocationKeysForTesting(ocId: differentAccount.ocId)?.parentLocation == differentAccount.serverUrl)
        #expect(manager.itemMetadata(ocId: differentAccount.ocId) == nil)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: differentAccount.remotePath()) == nil)
        #expect(manager.materialisedItemMetadatas()?.contains { $0.ocId == differentAccount.ocId } == false)
    }

    @Test func unboundLegacyStoreOnlyRebasesRowsForTheCurrentLogin() throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let current = Self.currentAccount
        let previousBackend = Account(
            user: current.username,
            id: Self.oldAccount.id,
            serverUrl: current.serverUrl,
            password: current.password
        )
        let oldFile = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            account: previousBackend
        )
        let foreignFile = DatabaseTestSuites.makeFile(
            ocId: "foreign-file-id",
            fileName: "foreign-report.pdf",
            account: Self.oldAccount
        )
        _ = try FilesDatabaseManager.writeUnboundStoreForTesting(
            at: directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension),
            metadata: [oldFile, foreignFile]
        )

        let manager = try openManager()

        #expect(manager.itemMetadata(locatedAtRemoteUrl: current.davFilesUrl + "/" + Self.fileName)?.ocId == Self.fileId)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: current.davFilesUrl + "/" + foreignFile.fileName) == nil)
        #expect(manager.itemMetadata(ocId: foreignFile.ocId) == nil)
        #expect(manager.itemMetadatas(account: Self.oldAccount.ncKitAccount).first { $0.ocId == foreignFile.ocId }?.remotePath() == foreignFile.remotePath())
    }

    @Test(arguments: [false, true])
    func mixedCacheRebasesOldRowsBeforeListing(fileUploadFailed: Bool) async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let account = Self.currentAccount
        let previousIdentity = try writeUnboundStoreWithMaterializedItems(
            oldAccount: Self.previousHostAccount,
            additionalMetadata: [
                DatabaseTestSuites.makeFile(
                    ocId: "already-current",
                    fileName: "Listed before the upgrade.txt",
                    account: account
                )
            ],
            fileUploadFailed: fileUploadFailed
        )
        savePreviouslyConfiguredAccount(account)

        let manager = try openManager()
        let folder = try #require(manager.itemMetadata(ocId: Self.folderId))
        let file = try #require(manager.itemMetadata(ocId: Self.fileId))
        let folderURL = account.davFilesUrl + "/" + Self.folderName

        #expect(manager.storeIdentity != previousIdentity)
        #expect(folder.remotePath() == folderURL)
        #expect(folder.visitedDirectory)
        #expect(file.remotePath() == folderURL + "/" + Self.fileName)
        #expect(file.account == account.ncKitAccount)
        #expect(file.downloaded)
        #expect(file.keepDownloaded)
        #expect(file.etag == Self.storedETag)
        #expect(file.hasUnfinishedUpload == fileUploadFailed)
        if fileUploadFailed {
            #expect(file.chunkUploadId == "pending-edit-upload")
        }
        #expect(manager.itemMetadatas(account: Self.previousHostAccount.ncKitAccount).isEmpty)

        let reopened = try openManager()
        #expect(reopened.storeIdentity == manager.storeIdentity)
        #expect(reopened.itemMetadata(ocId: file.ocId)?.remotePath() == file.remotePath())

        let remote = makeRemoteWithTeamFolder()
        remote.enumerateCallHandler = { path, depth, _, _, _, _, _, _ in
            #expect(path == folderURL)
            #expect(depth == .targetAndDirectChildren)
        }
        let listing = await listDirectory(folderURL, manager: reopened, remote: remote)
        #expect(listing.error == nil)
        #expect(remote.readOperationCount == 1)
        let listedFile = try #require(listing.metadatas?.first { $0.ocId == file.ocId })
        #expect(listedFile.etag == (fileUploadFailed ? Self.storedETag : Self.remoteETag))
        #expect(reopened.itemMetadata(ocId: file.ocId)?.keepDownloaded == true)
    }

    @Test func startupDoesNotInferThePreviousAccountWhenRowsUseMultiplePreviousServerURLs() throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let account = Self.currentAccount
        let currentFile = DatabaseTestSuites.makeFile(
            ocId: "current-server-file",
            fileName: "Current.txt",
            account: account
        )
        _ = try writeUnboundStoreWithMaterializedItems(
            oldAccount: Self.previousHostAccount,
            additionalMetadata: [
                currentFile,
                DatabaseTestSuites.makeFileFromEarlierServerURL(account: account)
            ]
        )
        savePreviouslyConfiguredAccount(account)

        let manager = try openManager()
        #expect(manager.itemMetadata(ocId: currentFile.ocId)?.remotePath() == currentFile.remotePath())
        #expect(manager.itemMetadata(ocId: Self.fileId) == nil)
        let oldFile = try #require(manager.itemMetadatas(account: Self.previousHostAccount.ncKitAccount).first {
            $0.ocId == Self.fileId
        })
        #expect(oldFile.urlBase == Self.previousHostAccount.serverUrl)
        #expect(oldFile.downloaded)
    }

    @Test func legacyAccountMatchingUsesTheOriginalLoginBytes() throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let previous = Account(user: "caf\u{00E9}", id: "backend-id", serverUrl: "https://example.com", password: "password")
        let current = Account(user: "cafe\u{0301}", id: previous.id, serverUrl: previous.serverUrl, password: previous.password)
        savePreviouslyConfiguredAccount(previous)

        var file = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            serverUrl: previous.davFilesUrl + "/Docs",
            account: previous
        )
        file.urlBase = ""
        file.userId = ""

        _ = try FilesDatabaseManager.writeUnboundStoreForTesting(
            at: directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension),
            metadata: [file]
        )

        let manager = try openManager(account: current)
        let restored = try #require(manager.itemMetadata(ocId: file.ocId))
        #expect(restored.account.utf8.elementsEqual(current.ncKitAccount.utf8))
        #expect(try manager.storedParentLocationKeysForTesting(ocId: file.ocId)?.parentLocation == "files:/Docs")
    }

    @Test(arguments: DAVAccountChange.allCases)
    func reopeningAfterDAVAccountChangeRebuildsParentURLsAndDropsQueuedDeletions(change: DAVAccountChange) throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let original = try openManager(account: Self.oldAccount)
        let oldParent = Self.oldAccount.davFilesUrl + "/" + Self.folderName
        var file = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            serverUrl: oldParent,
            account: Self.oldAccount
        )
        file.etag = Self.storedETag
        original.addItemMetadata(file)
        let staleSession = "pending-old-account-deletion"
        try #require(original.createChangeDeliverySession(
            sessionId: staleSession,
            containerKey: NSFileProviderItemIdentifier.workingSet.rawValue,
            anchorKey: "old-anchor",
            finalAnchorRawValue: Data(),
            updated: [],
            deleted: [file],
            incomplete: false,
            hardRemoveDeleted: true
        ))

        let stored = try #require(try original.storedParentLocationKeysForTesting(ocId: Self.fileId))
        #expect(stored.parentLocation == "files:/" + Self.folderName)

        let currentAccount = change.accountAfterChange
        let reopened = try openManager(account: currentAccount)

        #expect(reopened.storeIdentity != original.storeIdentity)
        #expect(try reopened.changeDeliverySession(sessionId: staleSession) == nil)
        let reopenedFile = try #require(reopened.itemMetadata(ocId: Self.fileId))
        #expect(!reopenedFile.deleted)
        #expect(reopenedFile.etag == Self.storedETag)
        #expect(reopenedFile.davParentPathWithoutOrigin == Self.filesRootPathWithoutOrigin(for: currentAccount) + "/" + Self.folderName + "/")
        #expect(reopenedFile.remotePath() == currentAccount.davFilesUrl + "/" + Self.folderName + "/" + Self.fileName)
    }

    @Test(arguments: QueuedParentFormat.allCases)
    func queuedUpdateRebuildsTheDAVParentURL(format: QueuedParentFormat) throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let account = format == .equivalentResponseWebroot
            ? Account(
                user: "alice",
                id: "alice",
                serverUrl: "https://example.com/caf%C3%A9",
                password: "password"
            )
            : Self.currentAccount
        let parentURL = format == .absoluteParentURL
            ? account.serverUrl + "/remote.php/webdav/Docs"
            : account.davFilesUrl + "/Docs"
        let file = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            serverUrl: parentURL,
            account: account
        )
        var cachedFile = file
        if format == .equivalentResponseWebroot {
            cachedFile.serverUrl = "https://example.com/cafe\u{0301}" + Account.webDavFilesUrlSuffix + account.id + "/Docs"
        }
        let sessionId = "queued-update"
        let identity = try FilesDatabaseManager.writeUnboundStoreWithQueuedUpdateForTesting(
            at: directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension),
            metadata: cachedFile,
            sessionId: sessionId
        )

        let manager = try openManager(account: account)

        #expect(manager.storeIdentity == identity)
        #expect(manager.itemMetadata(ocId: file.ocId)?.remotePath() == file.remotePath())
        let change = try #require(manager.changeDeliveryItems(sessionId: sessionId, fromSequence: 0, limit: 1)?.first)
        let snapshot = try #require(JSONSerialization.jsonObject(with: change.metadataData) as? [String: Any])
        #expect(snapshot["serverUrl"] as? String == (format == .absoluteParentURL ? parentURL : nil))
        #expect(snapshot["parentDAVPath"] as? String == (format == .absoluteParentURL ? nil : "files:/Docs"))
        try expectQueuedUpdateReplays(from: manager, file: file, sessionId: sessionId)
    }

    @Test func malformedQueueKeysDoNotPreventOtherChangesFromReplaying() throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let file = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            account: Self.currentAccount
        )
        let url = directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension)
        let sessionId = "queued-update"
        _ = try FilesDatabaseManager.writeUnboundStoreWithQueuedUpdateForTesting(
            at: url,
            metadata: file,
            sessionId: sessionId
        )

        var staleFile = file
        staleFile.apply(account: Self.oldAccount)
        staleFile.serverUrl = Self.oldAccount.davFilesUrl
        let staleSnapshot = try JSONEncoder().encode(staleFile)

        let queue = try DatabaseQueue(path: url.path)
        try queue.write { db in
            try db.execute(
                sql: """
                    INSERT INTO changeDeliveryItem (sessionId, sequence, metadataData, deleted)
                    VALUES (CAST(X'FE' AS TEXT), 0, ?, 1)
                    """,
                arguments: [staleSnapshot]
            )
            try db.execute(sql: """
                INSERT INTO changeDeliveryItem (sessionId, sequence, metadataData, deleted)
                VALUES (CAST(X'FF' AS TEXT), 0, X'FF', 0)
                """)
        }

        let manager = try openManager()
        try expectQueuedUpdateReplays(from: manager, file: file, sessionId: sessionId)
    }

    @Test(arguments: [
        ("https://example.com:8443", "alicé", true),
        ("https://example.com", "alicé", false),
        ("https://example.com:8443", "alice\u{0301}", false)
    ])
    func convertsCachedSDKWebrootAliases(
        responseOrigin: String,
        responseBackendID: String,
        shouldConvert: Bool
    ) throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let account = Account(
            user: "alice",
            id: "alicé",
            serverUrl: "https://example.com:8443/caf%C3%A9",
            password: "password"
        )
        let responseFilesURL = responseOrigin + "/cafe\u{0301}" + Account.webDavFilesUrlSuffix
        let responseParentURL = responseFilesURL + responseBackendID + "/Docs"
        let file = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            serverUrl: responseParentURL,
            account: account
        )
        var root = DatabaseTestSuites.makeFile(
            ocId: "cached-root",
            fileName: responseBackendID,
            serverUrl: String(responseFilesURL.dropLast()),
            account: account
        )
        root.directory = true
        let url = directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension)
        _ = try FilesDatabaseManager.writeUnboundStoreForTesting(
            at: url,
            metadata: [file, root]
        )

        let manager = try openManager(account: account)
        let storedFileParent = try manager.storedParentLocationKeysForTesting(ocId: file.ocId)
        let storedRootParent = try manager.storedParentLocationKeysForTesting(ocId: root.ocId)

        #expect(storedFileParent?.parentLocation == (shouldConvert ? "files:/Docs" : responseParentURL))
        #expect(storedRootParent?.parentLocation == (shouldConvert ? "files:" : root.serverUrl))

        let byURL = manager.itemMetadata(locatedAtRemoteUrl: account.davFilesUrl + "/Docs/" + file.fileName)
        #expect(byURL?.ocId == (shouldConvert ? file.ocId : nil))
        if shouldConvert {
            let restoredRoot = manager.rootItemMetadata()
            #expect(restoredRoot?.ocId == root.ocId)
            #expect(restoredRoot?.remotePath() == account.davFilesUrl)
        }
    }

    private func expectQueuedUpdateReplays(
        from manager: FilesDatabaseManager,
        file: SendableItemMetadata,
        sessionId: String
    ) throws {
        let buffer = ChangeDeliveryBuffer(
            dbManager: manager,
            containerKey: "working-set",
            hardRemoveDeleted: false,
            log: log
        )
        try #require(try buffer.isPrimed(forKey: "old-anchor"))
        let batch = try #require(buffer.prepareChangeDeliveryBatch(maxItems: 1))
        #expect(batch.updated.map(\.ocId) == [file.ocId])
        #expect(batch.updated.map { $0.remotePath() } == [file.remotePath()])
        #expect(batch.deleted.isEmpty)
        #expect(!batch.moreComing)
        buffer.acknowledgeBatch(batch.acknowledgement)
        #expect(try manager.changeDeliverySession(sessionId: sessionId) == nil)
    }

    @Test(arguments: [false, true])
    func bothDAVRootFormsRetainTheirItemIDAfterRebasing(usesNextcloudKitRootPlaceholder: Bool) throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let old = Account(
            user: "old-id",
            id: "old-id",
            serverUrl: "https://old.example.com/",
            password: "password"
        )
        let filesParentURL = "https://old.example.com" + String(Account.webDavFilesUrlSuffix.dropLast())
        var root = DatabaseTestSuites.makeFile(
            ocId: "server-root-id",
            fileName: usesNextcloudKitRootPlaceholder ? NextcloudKit.shared.nkCommonInstance.rootFileName : old.id,
            serverUrl: usesNextcloudKitRootPlaceholder ? filesParentURL + "/" + old.id : filesParentURL,
            account: old
        )
        root.directory = !usesNextcloudKitRootPlaceholder
        root.keepDownloaded = true
        _ = try FilesDatabaseManager.writeUnboundStoreForTesting(
            at: directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension),
            metadata: [root]
        )

        let account = Self.currentAccount
        savePreviouslyConfiguredAccount(old)
        let manager = try openManager()
        let migrated = try #require(manager.rootItemMetadata())
        let stored = try #require(try manager.storedParentLocationKeysForTesting(ocId: root.ocId))

        #expect(stored.parentLocation == "files:")
        #expect(migrated.account == account.ncKitAccount)
        #expect(migrated.ocId == root.ocId)
        #expect(migrated.directory)
        #expect(migrated.keepDownloaded)
        #expect(migrated.fileName == "/")
        #expect(migrated.remotePath() == account.davFilesUrl)
        #expect(manager.itemMetadata(locatedAtRemoteUrl: account.davFilesUrl)?.ocId == root.ocId)

        var synthetic = migrated
        synthetic.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue
        manager.addItemMetadata(synthetic)

        var child = DatabaseTestSuites.makeFile(ocId: Self.folderId, fileName: account.id, account: account)
        child.directory = true
        child.uploaded = true
        let changes = try #require(manager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: account.davFilesUrl,
            updatedMetadatas: [migrated, child],
            keepExistingDownloadState: true
        ))
        #expect(changes.deleted.isEmpty)
        #expect(manager.immediateChildItems(directoryMetadata: migrated).map(\.ocId) == [child.ocId])
        #expect(manager.childItemCount(directoryMetadata: migrated) == 1)

        let reopened = try openManager()
        #expect(reopened.itemMetadata(ocId: root.ocId)?.deleted == false)
        #expect(reopened.itemMetadata(ocId: synthetic.ocId)?.deleted == false)
    }

    @Test(arguments: EnumerationScope.allCases)
    func enumerationReadsExistingItemsAtTheCurrentDAVBase(container: EnumerationScope) async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        _ = try writeUnboundStoreWithMaterializedItems()
        let manager = try openManager()
        let remote = makeRemoteWithTeamFolder()
        let folderUrl = Self.currentAccount.davFilesUrl + "/" + Self.folderName
        remote.enumerateCallHandler = { path, _, _, _, _, _, _, _ in
            #expect(path == folderUrl || path == folderUrl + "/" + Self.fileName)
        }

        let observer = try await enumerateChanges(in: container, manager: manager, remote: remote)

        #expect(remote.readOperationCount > 0)
        #expect(observer.changedItems.contains { $0.itemIdentifier.rawValue == Self.fileId })
        #expect(observer.deletedItemIdentifiers.isEmpty)
        let file = try #require(manager.itemMetadata(ocId: Self.fileId))
        #expect(file.etag == Self.remoteETag)
        #expect(!file.deleted)
        #expect(file.downloaded)
        #expect(file.keepDownloaded)
    }

    @Test(arguments: EnumerationScope.allCases)
    func aMissingItemAtTheCurrentDAVBaseStillReportsDeletion(container: EnumerationScope) async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        _ = try writeUnboundStoreWithMaterializedItems()
        let manager = try openManager()
        let remote = MockRemoteInterface(account: Self.currentAccount, rootItem: .rootItem(account: Self.currentAccount))
        let folderUrl = Self.currentAccount.davFilesUrl + "/" + Self.folderName
        remote.enumerateCallHandler = { path, _, _, _, _, _, _, _ in
            #expect(path == folderUrl || path == folderUrl + "/" + Self.fileName)
        }

        let observer = try await enumerateChanges(in: container, manager: manager, remote: remote)

        #expect(remote.readOperationCount > 0)
        #expect(observer.deletedItemIdentifiers.contains(.init(Self.folderId)))
    }

    private func writeUnboundStoreWithMaterializedItems(
        oldAccount: Account = Self.oldAccount,
        additionalMetadata: [SendableItemMetadata] = [],
        fileUploadFailed: Bool = false
    ) throws -> String {
        savePreviouslyConfiguredAccount(oldAccount)
        let oldParent = oldAccount.davFilesUrl + "/" + Self.folderName
        var folder = DatabaseTestSuites.makeFile(
            ocId: Self.folderId,
            fileName: Self.folderName,
            account: oldAccount
        )
        folder.directory = true
        folder.visitedDirectory = true
        folder.uploaded = true
        folder.etag = Self.storedETag
        var file = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            serverUrl: oldParent,
            account: oldAccount
        )
        file.downloaded = true
        file.uploaded = true
        file.keepDownloaded = true
        file.etag = Self.storedETag
        if fileUploadFailed {
            file.uploaded = false
            file.status = Status.uploadError.rawValue
            file.chunkUploadId = "pending-edit-upload"
        }
        return try FilesDatabaseManager.writeUnboundStoreForTesting(
            at: directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension),
            metadata: [folder, file] + additionalMetadata
        )
    }

    private func savePreviouslyConfiguredAccount(_ account: Account) {
        var defaults = FileProviderDomainDefaults(identifier: domain, log: log, defaults: DatabaseTestSuites.defaults)
        defaults.serverUrl = account.serverUrl
        defaults.user = account.username
        defaults.userId = account.id
    }

    private func openManager(account: Account = Self.currentAccount) throws -> FilesDatabaseManager {
        try FilesDatabaseManager(
            account: account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: domain,
            log: log,
            defaults: DatabaseTestSuites.defaults
        )
    }

    private static func filesRootPathWithoutOrigin(for account: Account) -> String {
        URL(string: account.serverUrl)!.path + Account.webDavFilesUrlSuffix + account.id
    }

    private func makeRemoteWithTeamFolder() -> MockRemoteInterface {
        let account = Self.currentAccount
        let root = MockRemoteItem.rootItem(account: account)
        let folder = MockRemoteItem(
            identifier: Self.folderId,
            versionIdentifier: Self.remoteETag,
            name: Self.folderName,
            remotePath: account.davFilesUrl + "/" + Self.folderName,
            directory: true,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        let file = MockRemoteItem(
            identifier: Self.fileId,
            versionIdentifier: Self.remoteETag,
            name: Self.fileName,
            remotePath: folder.remotePath + "/" + Self.fileName,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        root.children = [folder]
        folder.parent = root
        folder.children = [file]
        file.parent = folder
        return MockRemoteInterface(account: account, rootItem: root)
    }

    private func listDirectory(
        _ url: String,
        manager: FilesDatabaseManager,
        remote: MockRemoteInterface
    ) async -> RemoteReadResult {
        await Enumerator.readServerUrl(
            url,
            account: Self.currentAccount,
            remoteInterface: remote,
            dbManager: manager,
            depth: .targetAndDirectChildren,
            log: log
        )
    }

    private func enumerateChanges(
        in container: EnumerationScope,
        manager: FilesDatabaseManager,
        remote: MockRemoteInterface
    ) async throws -> MockChangeObserver {
        let enumerator = try Enumerator(
            enumeratedItemIdentifier: container.identifier,
            account: Self.currentAccount,
            remoteInterface: remote,
            dbManager: manager,
            log: log
        )
        let observer = MockChangeObserver(enumerator: enumerator)
        let anchor = Enumerator.syncAnchor(at: Date(timeIntervalSinceNow: -60), storeIdentity: manager.storeIdentity)
        try await observer.enumerateChanges(from: anchor)
        return observer
    }
}
