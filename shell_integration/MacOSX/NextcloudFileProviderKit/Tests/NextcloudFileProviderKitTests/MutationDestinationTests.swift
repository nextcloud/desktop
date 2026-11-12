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

    @Test(arguments: Mutation.allCases, [true, false])
    func mutationsUseOnlyDestinationFoldersBoundToTheCurrentAccount(
        mutation: Mutation,
        hasMixedServerURLs: Bool
    ) async throws {
        let account = DatabaseTestSuites.account
        let previous = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://previous.example.invalid",
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
        var cachedItems = [cachedFolder]
        if hasMixedServerURLs {
            cachedItems.append(DatabaseTestSuites.makeFileFromEarlierServerURL(account: account))
        }
        _ = try FilesDatabaseManager.writeUnboundStoreForTesting(at: storeURL, metadata: cachedItems)
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
            if path == account.davFilesUrl {
                #expect(depth == .targetAndDirectChildren)
            } else {
                #expect(path == folderURL + "/" + createdFolderName)
                #expect(depth == .target)
            }
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

        if hasMixedServerURLs {
            let (modified, error) = await mutate()
            #expect((error as? NSFileProviderError)?.code == .cannotSynchronize)
            #expect(modified == nil)
            #expect(remote.readOperationCount == 0)
            #expect(manager.directoryMetadata(ocId: folderID) == nil)
            #expect(manager.itemMetadatas(account: previous.ncKitAccount).map(\.ocId) == [folderID])

            let listing = await Enumerator.readServerUrl(
                account.davFilesUrl,
                account: account,
                remoteInterface: remote,
                dbManager: manager,
                depth: .targetAndDirectChildren,
                log: log
            )
            try #require(listing.error == nil)
            #expect(remote.readOperationCount == 1)
            #expect(folder.children.isEmpty)
            #expect(root.children.contains { $0.identifier == source.identifier })
            #expect(manager.itemMetadatas(account: previous.ncKitAccount).isEmpty)
        }

        let (modified, error) = await mutate()
        #expect(error == nil)
        let recoveryReads = hasMixedServerURLs ? 1 : 0
        let newFolderReads = mutation == .createFolder ? 1 : 0
        #expect(remote.readOperationCount == recoveryReads + newFolderReads)
        let name = mutation == .createFolder ? createdFolderName : source.name
        #expect(modified?.metadata.remotePath() == folderURL + "/" + name)
        #expect(folder.children.map(\.name) == [name])
        #expect(manager.directoryMetadata(ocId: folderID)?.remotePath() == folderURL)
    }
}
