//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing
@testable import TestInterface

@Suite("WebDAV mutation destinations")
struct MutationDestinationTests {
    enum Mutation: CaseIterable {
        case createFolder
        case moveFile
    }

    enum DAVRootChange: CaseIterable {
        case serverURL
        case backendUserID
    }

    @Test(arguments: Mutation.allCases, DAVRootChange.allCases)
    func mutationsUseTheDestinationDAVRootSetAtStartup(
        mutation: Mutation,
        change: DAVRootChange
    ) async throws {
        let account = DatabaseTestSuites.account
        let previous = Account(
            user: account.username,
            id: change == .backendUserID ? "previous-backend-id" : account.id,
            serverUrl: change == .serverURL ? "https://previous.example.invalid" : account.serverUrl,
            password: account.password
        )
        let domain = NSFileProviderDomainIdentifier(UUID().uuidString)
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(domain.rawValue, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }

        let folderName = "Team folder"
        let folderID = "destination-folder-id"
        var cachedFolder = DatabaseTestSuites.makeFile(ocId: folderID, fileName: folderName, account: previous)
        cachedFolder.directory = true
        cachedFolder.uploaded = true
        let storeURL = directory.appendingPathComponent(domain.rawValue).appendingPathExtension(DatabaseSchema.fileExtension)
        _ = try FilesDatabaseManager.writeDatabaseWithoutSavedDAVAccountForTesting(
            at: storeURL,
            metadata: [cachedFolder]
        )
        let log = FileProviderLogMock()
        let manager = try FilesDatabaseManager(
            account: account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: domain,
            log: log,
            defaults: DatabaseTestSuites.defaults
        )
        let root = MockRemoteItem.rootItem(account: account)
        let folderURL = account.davFilesUrl + "/" + folderName
        try #require(manager.directoryMetadata(ocId: folderID)?.remotePath() == folderURL)
        let folder = MockRemoteItem(
            identifier: folderID,
            name: folderName,
            remotePath: folderURL,
            directory: true,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        let source = MockRemoteItem(
            identifier: "source-file-id",
            name: "Report.txt",
            remotePath: account.davFilesUrl + "/Report.txt",
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        root.children = [folder, source]
        folder.parent = root
        source.parent = root
        let remote = MockRemoteInterface(account: account, rootItem: root)
        let createdFolderName = "New folder"
        remote.enumerateCallHandler = { path, depth, _, _, _, requestedAccount, _, _ in
            #expect(path == folderURL + "/" + createdFolderName)
            #expect(depth == .target)
            #expect(requestedAccount == account)
        }

        func item(_ metadata: SendableItemMetadata, parent: NSFileProviderItemIdentifier) -> Item {
            Item(
                metadata: metadata,
                parentItemIdentifier: parent,
                account: account,
                remoteInterface: remote,
                dbManager: manager,
                displayFileActions: false,
                remoteSupportsTrash: false,
                log: log
            )
        }

        let parent = NSFileProviderItemIdentifier(folderID)
        let sourceMetadata = source.toItemMetadata(account: account)
        manager.addItemMetadata(sourceMetadata)

        func mutate() async -> (Item?, Error?) {
            switch mutation {
                case .createFolder:
                    var template = DatabaseTestSuites.makeFile(ocId: "local-new-folder", fileName: createdFolderName, account: account)
                    template.directory = true
                    return await Item.create(
                        basedOn: item(template, parent: parent),
                        contents: nil,
                        account: account,
                        remoteInterface: remote,
                        progress: Progress(),
                        dbManager: manager,
                        log: log
                    )
                case .moveFile:
                    return await item(sourceMetadata, parent: .rootContainer).modify(
                        itemTarget: item(sourceMetadata, parent: parent),
                        changedFields: [.parentItemIdentifier],
                        contents: nil,
                        dbManager: manager
                    )
            }
        }

        let stoppedRequest = try #require(manager.syncOperations.begin())
        defer { stoppedRequest.finish() }
        manager.syncOperations.stopAcceptingWork()
        await manager.syncOperations.waitForWrites()
        let (stoppedItem, stopError) = await SyncOperations.Operation.$current.withValue(stoppedRequest) {
            await mutate()
        }
        #expect(stoppedItem == nil)
        #expect((stopError as? NSFileProviderError)?.code == .serverUnreachable)
        #expect(folder.children.isEmpty)
        #expect(remote.readOperationCount == 0)
        try #require(manager.syncOperations.resumeAcceptingWork())

        let (modified, error) = await mutate()
        #expect(error == nil)
        let newFolderReads = mutation == .createFolder ? 1 : 0
        #expect(remote.readOperationCount == newFolderReads)
        let name = mutation == .createFolder ? createdFolderName : source.name
        #expect(modified?.metadata.remotePath() == folderURL + "/" + name)
        #expect(folder.children.map(\.name) == [name])
        #expect(manager.directoryMetadata(ocId: folderID)?.remotePath() == folderURL)
    }
}
