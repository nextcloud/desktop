//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
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

    @Test func reopeningUsesTheCurrentAccountForStoredPathsAndKeepsLocalState() throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let identity = try writeCachedFolderAndFile()

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

        let reopened = try openManager()

        #expect(reopened.storeIdentity == manager.storeIdentity)
        #expect(reopened.itemMetadata(ocId: Self.fileId)?.remotePath() == file.remotePath())
        #expect(try reopened.storedParentLocationKeysForTesting(ocId: Self.fileId)?.parentLocation == storedFile.parentLocation)
    }

    @Test(arguments: [false, true])
    func listingKeepsPendingUploadAfterStartupUpdatesDAVRoot(paginated: Bool) async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let account = Self.currentAccount
        let previous = Self.previousHostAccount
        var serverFile = DatabaseTestSuites.makeFile(
            ocId: "server-file",
            fileName: "Server.txt",
            serverUrl: previous.davFilesUrl + "/" + Self.folderName,
            account: previous
        )
        serverFile.uploaded = true
        _ = try writeCachedFolderAndFile(
            oldAccount: previous,
            additionalMetadata: [serverFile],
            fileUploadFailed: true
        )
        let manager = try openManager()
        let folderURL = account.davFilesUrl + "/" + Self.folderName
        let migratedPending = try #require(manager.itemMetadata(ocId: Self.fileId))
        #expect(migratedPending.remotePath() == folderURL + "/" + Self.fileName)
        #expect(migratedPending.hasUnfinishedUpload)
        #expect(manager.itemMetadata(ocId: serverFile.ocId)?.remotePath() == folderURL + "/" + serverFile.fileName)

        let remote = makeRemoteWithTeamFolder()
        remote.pagination = paginated
        let folder = try #require(remote.rootItem?.children.first)
        let listedFile = MockRemoteItem(
            identifier: serverFile.ocId,
            name: serverFile.fileName,
            remotePath: folder.remotePath + "/" + serverFile.fileName,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        folder.children.append(listedFile)
        listedFile.parent = folder

        let listing = await listDirectory(folder.remotePath, manager: manager, remote: remote, paginated: paginated)
        #expect(listing.error == nil)
        #expect(remote.readOperationCount == 1)
        #expect(listing.metadatas?.contains { $0.ocId == serverFile.ocId } == true)
        #expect(manager.itemMetadata(ocId: serverFile.ocId)?.remotePath() == listedFile.remotePath)
        let pending = try #require(manager.itemMetadata(ocId: Self.fileId))
        #expect(pending.remotePath() == folder.remotePath + "/" + Self.fileName)
        #expect(pending.hasUnfinishedUpload)
        #expect(pending.chunkUploadId == "pending-edit-upload")
        #expect(pending.etag == Self.storedETag)
        #expect(listing.metadatas?.first { $0.ocId == Self.fileId } == pending)
    }

    @Test func singleItemReadPreservesFolderFlagsChangedDuringTheRequest() async throws {
        defer { try? FileManager.default.removeItem(at: directory) }
        let account = Self.currentAccount
        let manager = try openManager()
        let remote = makeRemoteWithTeamFolder()
        let folder = try #require(remote.rootItem?.children.first)

        let response = folder.toItemMetadata(account: account)
        #expect(!response.keepDownloaded)
        #expect(!response.visitedDirectory)
        manager.addItemMetadata(response)
        _ = try manager.set(keepDownloaded: true, for: response)
        manager.setMaterializationStates([(ocId: response.ocId, downloaded: nil, visitedDirectory: true)])

        let reported = try await manager.addItemMetadataFromRemoteRead(
            response,
            requestedURL: folder.remotePath,
            isCancelled: { false }
        )
        #expect(!reported.isNew)
        #expect(reported.metadata.keepDownloaded)
        #expect(reported.metadata.visitedDirectory)
        let saved = try #require(manager.itemMetadata(ocId: folder.identifier))
        #expect(saved.keepDownloaded)
        #expect(saved.visitedDirectory)
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
        let interruptedUpload = try #require(try original.beginEditedItemUpload(
            ocId: file.ocId,
            uploadAttemptID: "previous-upload-attempt"
        ))
        #expect(interruptedUpload.session == "previous-upload-attempt")
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
        #expect(reopenedFile.hasUnfinishedUpload)
        #expect(reopenedFile.status == Status.uploadError.rawValue)
        #expect(reopenedFile.session == "")
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
        let file = DatabaseTestSuites.makeFile(
            ocId: Self.fileId,
            fileName: Self.fileName,
            serverUrl: account.davFilesUrl + "/Docs",
            account: account
        )
        var cachedFile = file
        if format == .equivalentResponseWebroot {
            cachedFile.serverUrl = "https://example.com/cafe\u{0301}" + Account.webDavFilesUrlSuffix + account.id + "/Docs"
        }
        let sessionId = "queued-update"
        let originalManager = try openManager(account: account)
        try queueUpdate(for: cachedFile, sessionId: sessionId, in: originalManager)

        let manager = try openManager(account: account)

        #expect(manager.storeIdentity == originalManager.storeIdentity)
        #expect(manager.itemMetadata(ocId: file.ocId)?.remotePath() == file.remotePath())
        let change = try #require(manager.changeDeliveryItems(sessionId: sessionId, fromSequence: 0, limit: 1)?.first)
        let snapshot = try #require(JSONSerialization.jsonObject(with: change.metadataData) as? [String: Any])
        #expect(snapshot["serverUrl"] == nil)
        #expect(snapshot["parentDAVPath"] as? String == "files:/Docs")
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
        let fixture = RealmStoreImporterTests.Fixture(directory: directory, domain: domain)
        try fixture.writeItems([file, root])

        let manager = try openManager(account: account)
        let storedFileParent = try manager.storedParentLocationKeysForTesting(ocId: file.ocId)
        let storedRootParent = try manager.storedParentLocationKeysForTesting(ocId: root.ocId)

        #expect(storedFileParent?.parentLocation == (shouldConvert ? "files:/Docs" : nil))
        #expect(storedRootParent?.parentLocation == (shouldConvert ? "files:" : nil))

        let byURL = manager.itemMetadata(locatedAtRemoteUrl: account.davFilesUrl + "/Docs/" + file.fileName)
        #expect(byURL?.ocId == (shouldConvert ? file.ocId : nil))
        if shouldConvert {
            let restoredRoot = manager.rootItemMetadata()
            #expect(restoredRoot?.ocId == root.ocId)
            #expect(restoredRoot?.remotePath() == account.davFilesUrl)
        }
    }

    private func queueUpdate(
        for file: SendableItemMetadata,
        sessionId: String,
        in manager: FilesDatabaseManager
    ) throws {
        manager.addItemMetadata(file)
        try #require(manager.createChangeDeliverySession(
            sessionId: sessionId,
            containerKey: "working-set",
            anchorKey: "old-anchor",
            finalAnchorRawValue: Data(),
            updated: [file],
            deleted: [],
            incomplete: false,
            hardRemoveDeleted: false
        ))
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
    func bothDAVRootFormsKeepTheirItemIDAfterDAVAccountChange(usesNextcloudKitRootPlaceholder: Bool) throws {
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
        let fixture = RealmStoreImporterTests.Fixture(directory: directory, domain: domain)
        try fixture.writeItems([root])

        let account = Self.currentAccount
        fixture.savePreviouslyConfiguredAccount(old)
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
        _ = try writeCachedFolderAndFile()
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
        _ = try writeCachedFolderAndFile()
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

    private func writeCachedFolderAndFile(
        oldAccount: Account = Self.oldAccount,
        additionalMetadata: [SendableItemMetadata] = [],
        fileUploadFailed: Bool = false
    ) throws -> String {
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
            serverUrl: oldAccount.davFilesUrl + "/" + Self.folderName,
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
        let manager = try openManager(account: oldAccount)
        for metadata in [folder, file] + additionalMetadata {
            manager.addItemMetadata(metadata)
        }
        return manager.storeIdentity
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
        remote: MockRemoteInterface,
        paginated: Bool = false
    ) async -> RemoteReadResult {
        await Enumerator.readServerUrl(
            url,
            pageSettings: paginated ? (page: nil, index: 0, size: 100) : nil,
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
