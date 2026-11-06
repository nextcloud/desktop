//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import Alamofire
@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
@testable import NextcloudKit
import RealmSwift
import Testing
@testable import TestInterface

private struct DAVReadFixture {
    let account: Account
    let manager: FilesDatabaseManager
    let folder: MockRemoteItem
    let file: MockRemoteItem
    let remote: MockRemoteInterface
    private let realm: Realm
    let anchorDate = Date(timeIntervalSinceNow: -60)

    static let defaultAccount = Account(user: "login", id: "user-id", serverUrl: "https://dav.example.invalid", password: "test")

    init(account: Account = Self.defaultAccount) {
        self.account = account
        manager = Self.makeManager(account: account)
        realm = manager.ncDatabase()
        let root = MockRemoteItem.rootItem(account: account)
        let folder = MockRemoteItem(
            identifier: "folder",
            versionIdentifier: "folder-v1",
            name: "folder",
            remotePath: account.davFilesUrl + "/folder",
            directory: true,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        let file = MockRemoteItem(
            identifier: "file",
            versionIdentifier: "file-v1",
            name: "file.txt",
            remotePath: folder.remotePath + "/file.txt",
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
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

    func storeFolderAndFile(at previousAccount: Account) throws {
        var storedFolder = try #require(manager.itemMetadata(ocId: folder.identifier))
        storedFolder.urlBase = previousAccount.serverUrl
        storedFolder.userId = previousAccount.id
        storedFolder.serverUrl = previousAccount.davFilesUrl
        manager.addItemMetadata(storedFolder)

        var storedFile = try #require(manager.itemMetadata(ocId: file.identifier))
        storedFile.urlBase = previousAccount.serverUrl
        storedFile.userId = previousAccount.id
        storedFile.serverUrl = storedFolder.remotePath()
        manager.addItemMetadata(storedFile)
    }

    static func makeManager(account: Account) -> FilesDatabaseManager {
        let identifier = UUID().uuidString
        return FilesDatabaseManager(
            realmConfiguration: Realm.Configuration(inMemoryIdentifier: identifier),
            account: account,
            databaseDirectory: FileManager.default.temporaryDirectory,
            fileProviderDomainIdentifier: .init(identifier),
            log: FileProviderLogMock()
        )
    }

    func makeEnumerator(_ identifier: NSFileProviderItemIdentifier) throws -> Enumerator {
        try Enumerator(
            enumeratedItemIdentifier: identifier,
            account: account,
            remoteInterface: remote,
            dbManager: manager,
            log: FileProviderLogMock()
        )
    }
}

// The fixtures use Realm’s process-wide default configuration.
extension BlockSyncTests {
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
        let body = Data("""
        <d:multistatus xmlns:d="DAV:" xmlns:oc="http://owncloud.org/ns">
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
        </d:multistatus>
        """.utf8)
        MockHTTPURLProtocol.setResponse(for: configuredURL, statusCode: 207, body: body)
        MockHTTPURLProtocol.setResponse(for: parsedURL, statusCode: 207, body: body)
        defer {
            MockHTTPURLProtocol.removeResponse(for: configuredURL)
            MockHTTPURLProtocol.removeResponse(for: parsedURL)
        }

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
        let initialRead = await remote.enumerate(remotePath: configuredURL.absoluteString, depth: .target, account: account)
        try #require(initialRead.error == .success)
        let metadata = try #require(initialRead.files.first).toItemMetadata()
        try #require(metadata.urlBase == account.serverUrl)
        try #require(metadata.remotePath() == parsedURL.absoluteString)

        let manager = DAVReadFixture.makeManager(account: account)
        let realm = manager.ncDatabase()
        defer { withExtendedLifetime(realm) {} }
        let result = await Enumerator.readServerUrl(
            metadata.remotePath(),
            account: account,
            sourceMetadata: metadata,
            remoteInterface: remote,
            dbManager: manager,
            depth: .target,
            log: FileProviderLogMock()
        )

        #expect(MockHTTPURLProtocol.requests(for: parsedURL).map(\.httpMethod) == ["PROPFIND"])
        #expect(result.error == nil)
        #expect(result.metadatas?.map(\.ocId) == [ocId])
    }

    @Test func workingSetSkipsAStaleURLThatWouldReturn404() async throws {
        let fixture = DAVReadFixture()
        let account = fixture.account
        let previousAccount = Account(
            user: account.username,
            id: account.id,
            serverUrl: "https://previous.example.com",
            password: account.password
        )
        try await expectWorkingSetToSkip404(at: previousAccount, fixture: fixture)
    }

    @Test func workingSetSkipsAStaleURLThatMatchesTheDAVResponseRoot() async throws {
        let account = DAVReadFixture.defaultAccount
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
        let fixture = DAVReadFixture(account: currentAccount)
        try await expectWorkingSetToSkip404(at: previousAccount, fixture: fixture)
    }

    private func expectWorkingSetToSkip404(at previousAccount: Account, fixture: DAVReadFixture) async throws {
        try fixture.storeFolderAndFile(at: previousAccount)
        let stalePath = previousAccount.davFilesUrl + "/" + fixture.folder.name
        let storedFolder = try #require(fixture.manager.itemMetadata(ocId: fixture.folder.identifier))
        try #require(storedFolder.urlBase == previousAccount.serverUrl)
        try #require(storedFolder.remotePath() == stalePath)
        fixture.remote.enumerateErrorBySuffix[stalePath] = NKError(statusCode: 404, fallbackDescription: "File not found")
        let anchor = Enumerator.syncAnchor(at: fixture.anchorDate)
        let observer = try MockChangeObserver(enumerator: fixture.makeEnumerator(.workingSet))

        try await observer.enumerateChanges(from: anchor)

        #expect(fixture.remote.readOperationCount == 0)
        #expect(observer.changedItems.isEmpty)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(observer.finishes.last?.anchor.rawValue == anchor.rawValue)
        #expect(fixture.manager.itemMetadata(ocId: fixture.folder.identifier)?.deleted == false)
        #expect(fixture.manager.itemMetadata(ocId: fixture.file.identifier)?.deleted == false)
    }

    @Test func containerSkipsAStaleURLThatWouldReturnAnEmpty207() async throws {
        let fixture = DAVReadFixture()
        let account = fixture.account
        let previousAccount = Account(
            user: account.username,
            id: account.id + "-previous",
            serverUrl: account.serverUrl,
            password: account.password
        )
        try fixture.storeFolderAndFile(at: previousAccount)
        // The mock serves the old collection without its cached child.
        fixture.remote.injectMock(previousAccount)
        fixture.remote.rootItem?.remotePath = previousAccount.davFilesUrl
        fixture.folder.remotePath = previousAccount.davFilesUrl + "/" + fixture.folder.name
        fixture.folder.userId = previousAccount.id
        fixture.folder.children = []
        fixture.remote.enumerateResponseStatusCode = 207
        let observer = try MockChangeObserver(enumerator: fixture.makeEnumerator(.init(fixture.folder.identifier)))

        await #expect(throws: NSFileProviderError(.cannotSynchronize)) {
            try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: fixture.anchorDate))
        }

        #expect(fixture.remote.readOperationCount == 0)
        #expect(observer.changedItems.isEmpty)
        #expect(observer.deletedItemIdentifiers.isEmpty)
        #expect(fixture.manager.itemMetadata(ocId: fixture.folder.identifier)?.deleted == false)
        #expect(fixture.manager.itemMetadata(ocId: fixture.file.identifier)?.deleted == false)
    }

    @Test func workingSetReportsA404UnderTheCurrentDAVBaseAsADeletion() async throws {
        let fixture = DAVReadFixture()
        fixture.remote.enumerateErrorBySuffix = ["/folder": NKError(statusCode: 404, fallbackDescription: "File not found")]
        let observer = try MockChangeObserver(enumerator: fixture.makeEnumerator(.workingSet))

        try await observer.enumerateChanges(from: Enumerator.syncAnchor(at: fixture.anchorDate))

        #expect(fixture.remote.readOperationCount == 1)
        #expect(observer.deletedItemIdentifiers == [.init(fixture.folder.identifier)])
    }

}
