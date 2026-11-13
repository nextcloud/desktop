//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import Alamofire
@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
@testable import NextcloudKit
import Testing
@testable import TestInterface

@Suite("Enumerator metadata persistence")
struct EnumeratorPersistenceTests {
    private let account: Account
    private let manager: FilesDatabaseManager
    private let folder: MockRemoteItem
    private let file: MockRemoteItem
    private let remote: MockRemoteInterface
    private let anchorDate = Date(timeIntervalSinceNow: -60)

    init() {
        self.init(account: DatabaseTestSuites.account)
    }

    private init(account: Account) {
        self.account = account
        manager = DatabaseTestSuites.makeManager(account: account)
        let root = MockRemoteItem.rootItem(account: account)
        let folder = MockRemoteItem(
            identifier: "folder", versionIdentifier: "folder-v1", name: "folder",
            remotePath: account.davFilesUrl + "/folder", directory: true,
            account: account.ncKitAccount, username: account.username,
            userId: account.id, serverUrl: account.serverUrl
        )
        let file = MockRemoteItem(
            identifier: "file", versionIdentifier: "file-v1", name: "file.txt",
            remotePath: folder.remotePath + "/file.txt",
            account: account.ncKitAccount, username: account.username,
            userId: account.id, serverUrl: account.serverUrl
        )
        folder.parent = root
        root.children = [folder]
        file.parent = folder
        folder.children = [file]
        self.folder = folder
        self.file = file
        remote = MockRemoteInterface(account: account, rootItem: root)

        var folderMetadata = folder.toItemMetadata(account: account)
        folderMetadata.visitedDirectory = true
        folderMetadata.syncTime = Date(timeIntervalSinceNow: -120)
        manager.addItemMetadata(folderMetadata)
        var fileMetadata = file.toItemMetadata(account: account)
        fileMetadata.syncTime = folderMetadata.syncTime
        manager.addItemMetadata(fileMetadata)
    }

    @Test func failedWriteReturnsAnErrorWithoutUnpersistedMetadata() async throws {
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(true)

        let result = await readFolder()

        #expect(remote.readOperationCount == 1)
        #expect(result.error == .metadataDatabaseWriteFailed)
        #expect(result.error?.fileProviderError?.code == .cannotSynchronize)
        #expect(result.metadatas == nil)
        #expect(result.changes == nil)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v1")
    }

    @Test(arguments: [false, true])
    func singleItemReadReportsMergedUpdate(saveFails: Bool) async throws {
        var cached = file.toItemMetadata(account: account)
        cached.downloaded = true
        cached.keepDownloaded = true
        manager.addItemMetadata(cached)
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(saveFails)

        let result = await Enumerator.readServerUrl(
            file.remotePath,
            remoteInterface: remote,
            dbManager: manager,
            depth: .target,
            log: FileProviderLogMock()
        )

        #expect(result.error == nil)
        let reported = try #require(result.metadatas?.first)
        let changes = try #require(result.changes)
        #expect(changes.created.isEmpty)
        #expect(changes.updated == [reported])
        #expect(reported.downloaded)
        #expect(reported.keepDownloaded)
        #expect(reported.etag == "file-v2")
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == (saveFails ? "file-v1" : "file-v2"))
    }

    @Test(arguments: [false, true])
    func singleItemReadReportsCreatedItemWithInheritedPin(saveFails: Bool) async throws {
        manager.removeItemMetadata(ocId: file.identifier)
        _ = try manager.set(keepDownloaded: true, for: folder.toItemMetadata(account: account))
        try manager.setItemMetadataWriteFailureForTesting(saveFails)

        let result = await Enumerator.readServerUrl(
            file.remotePath,
            remoteInterface: remote,
            dbManager: manager,
            depth: .target,
            log: FileProviderLogMock()
        )

        #expect(result.error == nil)
        let reported = try #require(result.metadatas?.first)
        let changes = try #require(result.changes)
        #expect(changes.created == [reported])
        #expect(changes.updated.isEmpty)
        #expect(reported.keepDownloaded)
        let saved = manager.itemMetadata(ocId: file.identifier)
        if saveFails {
            #expect(saved == nil)
        } else {
            #expect(saved?.keepDownloaded == true)
        }
    }

    @Test func folderListingReportsNewTargetWithInheritedPin() async throws {
        var root = try #require(folder.parent).toItemMetadata(account: account)
        root.keepDownloaded = true
        manager.addItemMetadata(root)
        manager.removeItemMetadata(ocId: folder.identifier)

        let result = await readFolder()

        #expect(result.error == nil)
        let reported = try #require(result.metadatas?.first)
        let saved = try #require(manager.itemMetadata(ocId: folder.identifier))
        #expect(reported.ocId == folder.identifier)
        #expect(reported.keepDownloaded)
        #expect(reported.visitedDirectory)
        #expect(saved.keepDownloaded)
        #expect(saved.visitedDirectory)
    }

    @Test func refusesDAVReadsBeforeReplacingTheAccountSession() async throws {
        let observer = try MockEnumerationObserver(enumerator: makeEnumerator(.rootContainer))
        manager.invalidateForDAVAccountChange()

        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await observer.enumerateItems()
        }
        #expect(remote.readOperationCount == 0)
        #expect(observer.items.isEmpty)
    }

    @Test func accountChangeCancelsActiveEnumerationWithoutWaitingForItsResponse() async throws {
        file.versionIdentifier = "file-v2"
        let (requestStarted, reportRequestStarted) = AsyncStream<Void>.makeStream()
        let (resumeRead, releaseRead) = AsyncStream<Void>.makeStream()
        defer { releaseRead.finish() }
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let url = try #require(URL(string: folder.remotePath))
        let (networkTask, networkCompletion) = observedNetworkTask(in: session, url: url)
        remote.enumerateCallHandler = { _, _, _, _, _, _, _, taskHandler in taskHandler(networkTask) }
        remote.enumerateHandler = {
            reportRequestStarted.yield(())
            reportRequestStarted.finish()
            // Simulate a response already queued for delivery when cancellation arrives.
            await Task.detached {
                var iterator = resumeRead.makeAsyncIterator()
                _ = await iterator.next()
            }.value
            #expect(Task.isCancelled)
        }

        let enumerator = try makeEnumerator(.init(folder.identifier))
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        let observer = MockChangeObserver(enumerator: enumerator)
        let enumeration = Task {
            do {
                try await observer.enumerateChanges(from: anchor)
                return nil as NSFileProviderError.Code?
            } catch {
                return (error as? NSFileProviderError)?.code
            }
        }
        defer { enumeration.cancel() }
        try #require(try await nextTestValue(from: requestStarted) != nil)

        let operations = manager.syncOperations
        operations.stopAcceptingWork()
        let rejected = MockChangeObserver(enumerator: enumerator)
        await #expect(throws: NSFileProviderError(.serverUnreachable)) {
            try await rejected.enumerateChanges(from: anchor)
        }
        #expect(remote.readOperationCount == 1)

        let drain = Task { await operations.waitForWrites() }
        try await testTaskValue(of: drain)
        #expect(try await nextTestValue(from: networkCompletion) == NSURLErrorCancelled)
        let incomingAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://replacement.example.invalid",
            password: account.password
        )
        let replacement = try manager.replacingDAVAccount(with: incomingAccount)
        manager.invalidateForDAVAccountChange()
        releaseRead.finish()

        #expect(try await testTaskValue(of: enumeration) == .syncAnchorExpired)
        #expect(observer.finishes.isEmpty)
        #expect(observer.changedItems.isEmpty)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(replacement.itemMetadata(ocId: file.identifier)?.etag == "file-v1")
    }

    @Test func workingSetKeepsItsAnchorAndReportsTheChangeAfterRetry() async throws {
        folder.versionIdentifier = "folder-v2"
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(true)
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        let failed = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))

        try await failed.enumerateChanges(from: anchor)

        #expect(failed.error == nil)
        #expect(failed.changedItems.isEmpty)
        #expect(failed.deletedItemIdentifiers.isEmpty)
        #expect(failed.finishes.last?.anchor.rawValue == anchor.rawValue)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v1")

        try manager.setItemMetadataWriteFailureForTesting(false)
        let recovered = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))
        try await recovered.enumerateChanges(from: anchor)

        #expect(recovered.error == nil)
        #expect(recovered.changedItems.contains { $0.itemIdentifier.rawValue == file.identifier })
        #expect(recovered.deletedItemIdentifiers.isEmpty)
        let recoveredAnchor = try #require(recovered.finishes.last?.anchor)
        #expect(recoveredAnchor.rawValue != anchor.rawValue)
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v2")
    }

    @Test func containerChangeEnumerationReportsThePersistenceError() async throws {
        file.versionIdentifier = "file-v2"
        try manager.setItemMetadataWriteFailureForTesting(true)
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.init(folder.identifier)))

        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity))
        }

        let error = try #require(observer.error) as NSError
        #expect(error.domain == NSFileProviderErrorDomain)
        #expect(error.code == NSFileProviderError.Code.cannotSynchronize.rawValue)
        #expect(observer.finishes.isEmpty)
        #expect(observer.changedItems.isEmpty)
        #expect(observer.deletedItemIdentifiers.isEmpty)
    }

    @Test func unchangedDirectoryReturnsAnEmptyChangeSet() async throws {
        let result = await readFolder()

        #expect(result.error == nil)
        #expect(result.metadatas?.count == 2)
        let changes = try #require(result.changes)
        #expect(changes.created.isEmpty)
        #expect(changes.updated.isEmpty)
        #expect(changes.deleted.isEmpty)
    }

    @Test func successfulWritePersistsAndReportsTheChangedFile() async throws {
        file.versionIdentifier = "file-v2"

        let result = await readFolder()

        #expect(result.error == nil)
        let changes = try #require(result.changes)
        #expect(changes.updated.contains { $0.ocId == file.identifier && $0.etag == "file-v2" })
        #expect(manager.itemMetadata(ocId: file.identifier)?.etag == "file-v2")
    }

    @Test(
        arguments: [false, true],
        [
            (renamed: false, moved: false, replacedAtOldName: false),
            (renamed: true, moved: false, replacedAtOldName: false),
            (renamed: false, moved: true, replacedAtOldName: false),
            (renamed: true, moved: false, replacedAtOldName: true)
        ]
    )
    func listingPreservesPendingUploadAndReportsSiblings(
        paginated: Bool,
        serverChange: (renamed: Bool, moved: Bool, replacedAtOldName: Bool)
    ) async throws {
        var pending = try #require(manager.itemMetadata(ocId: file.identifier))
        pending.status = Status.uploadError.rawValue
        pending.uploaded = false
        pending.etag = "local-edit"
        pending.chunkUploadId = "pending-edit-upload"
        pending.keepDownloaded = true
        pending.downloaded = true
        manager.addItemMetadata(pending)
        remote.pagination = paginated

        let listedFolderURL = serverChange.moved ? account.davFilesUrl + "/destination" : folder.remotePath
        if serverChange.moved {
            let created = await remote.createFolder(remotePath: listedFolderURL, account: account)
            try #require(created.error == .success)
        }
        if serverChange.moved || serverChange.renamed {
            let serverName = serverChange.renamed ? "renamed.txt" : file.name
            let moved = await remote.move(
                remotePathSource: file.remotePath,
                remotePathDestination: listedFolderURL + "/" + serverName,
                account: account
            )
            try #require(moved.error == .success)
            #expect(file.identifier == pending.ocId)
        }
        let listedFolder = try #require(remote.rootItem?.children.first { $0.remotePath == listedFolderURL })
        let siblingID = "sibling"
        let replacementID = "replacement"
        var otherItems = [(siblingID, "sibling.txt")]
        if serverChange.replacedAtOldName {
            otherItems.append((replacementID, pending.fileName))
        }
        for (id, name) in otherItems {
            let item = MockRemoteItem(
                identifier: id,
                name: name,
                remotePath: listedFolderURL + "/" + name,
                account: account.ncKitAccount,
                username: account.username,
                userId: account.id,
                serverUrl: account.serverUrl
            )
            item.parent = listedFolder
            listedFolder.children.append(item)
        }

        // The replacement arrives on a later page than the renamed item.
        let splitAcrossPages = paginated && serverChange.replacedAtOldName
        let pageSize = splitAcrossPages ? 3 : 10
        let result = await Enumerator.readServerUrl(
            listedFolderURL,
            pageSettings: paginated ? (nil, 0, pageSize) : nil,
            remoteInterface: remote,
            dbManager: manager,
            depth: .targetAndDirectChildren,
            log: FileProviderLogMock()
        )

        #expect(result.error == nil)
        var reportedItems = try #require(result.metadatas)
        if splitAcrossPages {
            let continuation = try #require(result.nextPage)
            let token = try #require(continuation.token)
            let nextPage = NSFileProviderPage(Data(token.utf8))
            let lastResult = await Enumerator.readServerUrl(
                listedFolderURL,
                pageSettings: (nextPage, continuation.index, pageSize),
                remoteInterface: remote,
                dbManager: manager,
                depth: .targetAndDirectChildren,
                log: FileProviderLogMock()
            )
            #expect(lastResult.error == nil)
            #expect(lastResult.nextPage == nil)
            reportedItems += try #require(lastResult.metadatas)
        }
        #expect(remote.readOperationCount == (splitAcrossPages ? 2 : 1))
        #expect(reportedItems.contains { $0.ocId == siblingID })
        #expect(!reportedItems.contains { $0.ocId == replacementID })
        if serverChange.replacedAtOldName {
            let replacement = try #require(manager.itemMetadata(ocId: replacementID))
            #expect(!replacement.deleted)
            #expect(!replacement.downloaded)
            #expect(replacement.remotePath() == pending.remotePath())
        }
        if serverChange.moved {
            #expect(!reportedItems.contains { $0.ocId == pending.ocId })
        } else {
            let reported = try #require(reportedItems.first { $0.ocId == pending.ocId })
            #expect(reported.remotePath() == pending.remotePath())
            #expect(reported.hasUnfinishedUpload)
            #expect(reported.etag == pending.etag)
        }
        #expect(result.changes?.deleted.contains { $0.ocId == pending.ocId } != true)

        let saved = try #require(manager.itemMetadata(ocId: pending.ocId))
        #expect(saved.remotePath() == pending.remotePath())
        #expect(saved.hasUnfinishedUpload)
        #expect(!saved.deleted)
        #expect(saved.etag == pending.etag)
        #expect(saved.chunkUploadId == pending.chunkUploadId)
        #expect(saved.keepDownloaded)
    }

    @Test func workingSetDoesNotReadOldPathOfRenamedPendingUpload() async throws {
        var pending = try #require(manager.itemMetadata(ocId: file.identifier))
        pending.status = Status.uploadError.rawValue
        pending.uploaded = false
        pending.downloaded = true
        manager.addItemMetadata(pending)
        let moved = await remote.move(
            remotePathSource: file.remotePath,
            remotePathDestination: folder.remotePath + "/renamed.txt",
            account: account
        )
        try #require(moved.error == .success)

        let observer = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        try await observer.enumerateChanges(from: anchor)

        #expect(observer.error == nil)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(remote.readOperationCount == 1)
        let saved = try #require(manager.itemMetadata(ocId: pending.ocId))
        #expect(saved.remotePath() == pending.remotePath())
        #expect(saved.hasUnfinishedUpload)
    }

    @Test func directoryListingPreservesChildrenStoredUnderTheDAVResponseURL() async throws {
        let account = Account(
            user: "login",
            id: "backend-id",
            serverUrl: "https://example.com/instance//nextcloud",
            password: "test"
        )
        let responseFilesRoot = "https://example.com/instance/nextcloud" + Account.webDavFilesUrlSuffix + account.id
        let responseDirectoryURL = responseFilesRoot + "/Docs"
        let requestedDirectoryURL = account.davFilesUrl + "/Docs"
        let manager = DatabaseTestSuites.makeManager(account: account)
        var cached = DatabaseTestSuites.makeFile(
            ocId: "report-id",
            fileName: "Report.txt",
            serverUrl: responseDirectoryURL,
            account: account
        )
        cached.uploaded = true
        cached.downloaded = true
        cached.keepDownloaded = true
        cached.etag = "old-etag"
        try manager.insertForTesting(cached)

        var directory = NKFile()
        directory.ocId = "docs-id"
        directory.directory = true
        directory.fileName = "Docs"
        directory.serverUrl = responseFilesRoot
        directory.account = account.ncKitAccount
        directory.urlBase = account.serverUrl
        directory.user = account.username
        directory.userId = account.id

        var report = directory
        report.ocId = cached.ocId
        report.directory = false
        report.fileName = cached.fileName
        report.serverUrl = responseDirectoryURL
        report.etag = "new-etag"

        let result = await Enumerator.handleDepth1ReadFileOrFolder(
            serverUrl: requestedDirectoryURL,
            dbManager: manager,
            files: [directory, report],
            pageIndex: nil,
            preserveDeletedTarget: false,
            log: FileProviderLogMock()
        )

        #expect(result.readError == nil)
        let stored = try #require(manager.itemMetadata(ocId: cached.ocId))
        #expect(stored.etag == report.etag)
        #expect(stored.downloaded)
        #expect(stored.keepDownloaded)
    }

    @Test func serverErrorIsPreserved() async {
        let error = NKError(statusCode: 503, fallbackDescription: "Unavailable")
        remote.enumerateErrorBySuffix = ["/folder": error]

        let result = await readFolder()

        #expect(result.error == error)
        #expect(result.metadatas == nil)
        #expect(result.changes == nil)
    }

    @Test(arguments: [
        ("/instance//nextcloud", "/instance//nextcloud"),
        ("/instance/./nextcloud", "/instance/nextcloud"),
        ("/instance/previous/../nextcloud", "/instance/nextcloud")
    ])
    func readsThePathProducedByTheDAVResponseParser(configuredWebroot: String, responseWebroot: String) async throws {
        let origin = "https://\(UUID().uuidString.lowercased()).example.invalid"
        let account = Account(
            user: "login",
            id: "user-id",
            serverUrl: origin + configuredWebroot,
            password: "test"
        )
        let filePath = Account.webDavFilesUrlSuffix + account.id + "/Docs/report.txt"
        let configuredURL = try #require(URL(string: account.serverUrl + filePath))
        let parsedURL = try #require(URL(string: origin + "/instance/nextcloud" + filePath))
        let ocId = "parser-file-id"
        let fileResponse = """
          <d:response>
            <d:href>\(responseWebroot)\(filePath)</d:href>
            <d:propstat>
              <d:prop>
                <oc:id>\(ocId)</oc:id>
                <d:getcontenttype>text/plain</d:getcontenttype>
              </d:prop>
              <d:status>HTTP/1.1 200 OK</d:status>
            </d:propstat>
          </d:response>
        """
        let body = Data("""
        <d:multistatus xmlns:d="DAV:" xmlns:oc="http://owncloud.org/ns">
        \(fileResponse)
        </d:multistatus>
        """.utf8)
        MockHTTPURLProtocol.setResponse(for: configuredURL, statusCode: 207, body: body)
        MockHTTPURLProtocol.setResponse(for: parsedURL, statusCode: 207, body: body)
        defer {
            MockHTTPURLProtocol.removeResponse(for: configuredURL)
            MockHTTPURLProtocol.removeResponse(for: parsedURL)
        }

        let remote = nextcloudKitForDAVRead(account: account)
        let initialRead = await remote.enumerate(remotePath: configuredURL.absoluteString, depth: .target, account: account)
        try #require(initialRead.error == .success)
        let file = try #require(initialRead.files.first)
        try #require(file.matchesResponseItemURL(parsedURL.absoluteString))
        let metadata = file.toItemMetadata()
        try #require(metadata.urlBase == account.serverUrl)

        let manager = DatabaseTestSuites.makeManager(account: account)
        let result = await Enumerator.readServerUrl(
            parsedURL.absoluteString,
            sourceMetadata: metadata,
            remoteInterface: remote,
            dbManager: manager,
            depth: .target,
            log: FileProviderLogMock()
        )
        #expect(result.error == nil)
        #expect(result.metadatas?.map(\.ocId) == [ocId])
    }

    @Test(arguments: [(false, true), (true, true), (false, false), (true, false)])
    func directoryReadAcceptsEquivalentWebrootWithMatchingBackendID(filesRoot: Bool, matchingBackendID: Bool) async throws {
        let origin = "https://\(UUID().uuidString.lowercased()).example.invalid"
        let account = Account(
            user: "login",
            id: "backénd",
            serverUrl: origin + "/N%C3%A9xt",
            password: "test"
        )
        let directorySuffix = filesRoot ? "" : "/Docs"
        let directoryURL = account.davFilesUrl + directorySuffix
        let requestURL = try #require(URL(string: directoryURL))
        let responseBackendID = matchingBackendID ? "back%C3%A9nd" : "backe%CC%81nd"
        let responsePath = "/Ne%CC%81xt" + Account.webDavFilesUrlSuffix + responseBackendID + directorySuffix
        let body = Data("""
        <d:multistatus xmlns:d="DAV:" xmlns:oc="http://owncloud.org/ns">
          <d:response>
            <d:href>\(responsePath)/</d:href>
            <d:propstat>
              <d:prop>
                <oc:id>directory-id</oc:id>
                <d:resourcetype><d:collection/></d:resourcetype>
              </d:prop>
              <d:status>HTTP/1.1 200 OK</d:status>
            </d:propstat>
          </d:response>
          <d:response>
            <d:href>\(responsePath)/report.txt</d:href>
            <d:propstat>
              <d:prop>
                <oc:id>file-id</oc:id>
                <d:getcontenttype>text/plain</d:getcontenttype>
              </d:prop>
              <d:status>HTTP/1.1 200 OK</d:status>
            </d:propstat>
          </d:response>
        </d:multistatus>
        """.utf8)
        MockHTTPURLProtocol.setResponse(for: requestURL, statusCode: 207, body: body)
        defer { MockHTTPURLProtocol.removeResponse(for: requestURL) }

        let manager = DatabaseTestSuites.makeManager(account: account)
        let result = await Enumerator.readServerUrl(
            directoryURL,
            remoteInterface: nextcloudKitForDAVRead(account: account),
            dbManager: manager,
            depth: .targetAndDirectChildren,
            log: FileProviderLogMock()
        )

        let requests = MockHTTPURLProtocol.requests(for: requestURL)
        #expect(requests.map(\.httpMethod) == ["PROPFIND"])
        #expect(requests.first?.value(forHTTPHeaderField: "Depth") == "1")
        if matchingBackendID {
            #expect(result.error == nil)
            #expect(result.metadatas?.map(\.ocId) == ["directory-id", "file-id"])
            let storedFile = manager.itemMetadata(locatedAtRemoteUrl: directoryURL + "/report.txt")
            #expect(storedFile?.ocId == "file-id")
            #expect(storedFile?.serverUrl.utf8.elementsEqual(directoryURL.utf8) == true)
        } else {
            #expect(result.error == .invalidData)
            #expect(manager.itemMetadata(ocId: "file-id") == nil)
        }
    }

    @Test func davReadRejectsThePreviousServerURL() async throws {
        let previousAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://previous.example.com",
            password: account.password
        )
        try await expectDAVReadToRejectStaleURL(at: previousAccount)
    }

    @Test func davReadRejectsThePreviousURLThatMatchesTheResponseWebroot() async throws {
        let currentAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: account.serverUrl + "/instance//nextcloud",
            password: account.password
        )
        let previousAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: account.serverUrl + "/instance/nextcloud",
            password: account.password
        )
        let fixture = Self(account: currentAccount)
        try await fixture.expectDAVReadToRejectStaleURL(at: previousAccount)
    }

    private func nextcloudKitForDAVRead(account: Account) -> NextcloudKit {
        let remote = NextcloudKit()
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MockHTTPURLProtocol.self]
        let session = NKSession(
            nkCommonInstance: remote.nkCommonInstance,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id,
            password: account.password,
            account: account.ncKitAccount,
            userAgent: "FileProvider-test",
            groupIdentifier: "",
            httpMaximumConnectionsPerHost: 1,
            httpMaximumConnectionsPerHostInDownload: 1,
            httpMaximumConnectionsPerHostInUpload: 1,
            sessionDataOverride: Alamofire.Session(configuration: configuration)
        )
        remote.nkCommonInstance.nksessions = SynchronizedNKSessionArray([session])
        return remote
    }

    private func expectDAVReadToRejectStaleURL(at previousAccount: Account) async throws {
        var staleFolder = try #require(manager.itemMetadata(ocId: folder.identifier))
        staleFolder.apply(account: previousAccount)
        staleFolder.serverUrl = previousAccount.davFilesUrl
        let stalePath = staleFolder.remotePath()
        remote.enumerateErrorBySuffix[stalePath] = NKError(statusCode: 404, fallbackDescription: "File not found")

        let result = await Enumerator.readServerUrl(
            stalePath,
            sourceMetadata: staleFolder,
            remoteInterface: remote,
            dbManager: manager,
            log: FileProviderLogMock()
        )

        #expect(result.error == .invalidData)
        #expect(remote.readOperationCount == 0)
        #expect(manager.itemMetadata(ocId: folder.identifier)?.deleted == false)
        #expect(manager.itemMetadata(ocId: file.identifier)?.deleted == false)
    }

    @Test(arguments: [NSFileProviderItemIdentifier.rootContainer.rawValue, "server-root-id"])
    func workingSetRetriesA404ForTheFilesRoot(rootID: String) async throws {
        var root = DatabaseTestSuites.makeFile(
            ocId: rootID,
            fileName: SendableItemMetadata.filesRootPlaceholderName,
            serverUrl: account.davFilesUrl,
            account: account
        )
        root.directory = true
        root.visitedDirectory = true
        root.keepDownloaded = true
        root.syncTime = Date(timeIntervalSinceNow: -120)
        manager.addItemMetadata(root)
        remote.enumerateErrorBySuffix[account.davFilesUrl] = NKError(statusCode: 404, fallbackDescription: "File not found")
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))

        try await observer.enumerateChanges(from: anchor)

        #expect(remote.readOperationCount == 2)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(observer.finishes.last?.anchor.rawValue == anchor.rawValue)
        let savedRoot = try #require(manager.itemMetadata(ocId: rootID))
        #expect(!savedRoot.deleted)
        #expect(savedRoot.keepDownloaded)
    }

    @Test func rootContainerReturnsARetryableErrorWhenDAVReturns404() async throws {
        remote.enumerateErrorBySuffix[account.davFilesUrl] = NKError(statusCode: 404, fallbackDescription: "File not found")
        let anchor = Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity)
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.rootContainer))

        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await observer.enumerateChanges(from: anchor)
        }

        #expect(remote.readOperationCount == 1)
        #expect(observer.deletedItemIdentifiers.isEmpty)
    }

    @Test func workingSetReportsA404UnderTheCurrentDAVBaseAsADeletion() async throws {
        remote.enumerateErrorBySuffix = ["/folder": NKError(statusCode: 404, fallbackDescription: "File not found")]
        let observer = try MockChangeObserver(enumerator: makeEnumerator(.workingSet))
        observer.beforeFinishEnumeratingChanges = {
            // Finishing the batch acknowledges the deletion and removes the row.
            let afterScan = Date()
            let changes = manager.pendingWorkingSetChanges(since: anchorDate)
            guard let deletion = changes?.deleted.first(where: { $0.ocId == folder.identifier }) else {
                Issue.record("The working-set scan did not save a deletion after the incoming anchor.")
                return
            }
            #expect(deletion.syncTime > anchorDate)
            #expect(deletion.syncTime <= afterScan)
            #expect(manager.pendingWorkingSetChanges(since: afterScan)?.deleted.isEmpty == true)
        }

        try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: anchorDate, storeIdentity: manager.storeIdentity))

        #expect(remote.readOperationCount == 1)
        #expect(observer.deletedItemIdentifiers == [.init(folder.identifier)])
    }

    private func readFolder() async -> RemoteReadResult {
        await Enumerator.readServerUrl(
            folder.remotePath,
            remoteInterface: remote,
            dbManager: manager,
            depth: .targetAndDirectChildren,
            log: FileProviderLogMock()
        )
    }

    private func makeEnumerator(_ identifier: NSFileProviderItemIdentifier) throws -> Enumerator {
        try Enumerator(
            enumeratedItemIdentifier: identifier,
            remoteInterface: remote,
            dbManager: manager,
            log: FileProviderLogMock()
        )
    }
}
