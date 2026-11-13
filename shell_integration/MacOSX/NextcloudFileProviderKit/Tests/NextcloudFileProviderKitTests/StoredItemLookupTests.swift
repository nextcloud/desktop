//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import Alamofire
@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
@testable import NextcloudKit
import Testing

struct StoredItemLookupTests {
    enum ChildState: CaseIterable {
        case live, deleted, absent
    }

    @Test(arguments: ChildState.allCases)
    func itemLookupDistinguishesAnUnresolvedParentFromAMissingChild(state: ChildState) async throws {
        let account = Account(user: UUID().uuidString, id: "current", serverUrl: "https://lookup.invalid", password: "password")
        let previous = Account(user: account.username, id: "previous", serverUrl: account.serverUrl, password: account.password)
        let domain = NSFileProviderDomain(identifier: .init(UUID().uuidString), displayName: "Lookup test")
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let manager = try FilesDatabaseManager(
            account: account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: domain.identifier,
            log: FileProviderLogMock()
        )
        let provider = FileProviderExtension(domain: domain)
        defer { provider.invalidate() }
        provider.ncAccount = account
        provider.dbManager = manager

        let identifier = NSFileProviderItemIdentifier("known-child")
        if state != .absent {
            var child = DatabaseTestSuites.makeFile(
                ocId: identifier.rawValue,
                fileName: "report.pdf",
                serverUrl: previous.davFilesUrl + "/uncached-parent",
                account: previous
            )
            child.deleted = state == .deleted
            try manager.insertForTesting(child)
            try #require(manager.parentItemIdentifierFromMetadata(child) == nil)
        }

        let (hasItem, errorCode): (Bool, NSFileProviderError.Code?) = await withCheckedContinuation { continuation in
            _ = provider.item(for: identifier, request: NSFileProviderRequest()) { item, error in
                continuation.resume(returning: (item != nil, (error as? NSFileProviderError)?.code))
            }
        }

        #expect(!hasItem)
        #expect(errorCode == (state == .live ? .cannotSynchronize : .noSuchItem))
        if state == .live {
            #expect(manager.itemMetadata(identifier)?.deleted == false)
        }
    }
}

/// The mutation callbacks read the same process-wide blockSync setting as the tests in this serialized suite.
extension BlockSyncTests {
    enum StoredItemMutation {
        case modify, delete
    }

    @Test(arguments: [
        (StoredItemMutation.modify, false),
        (.modify, true),
        (.delete, true)
    ])
    func mutationDistinguishesAnUnresolvedParentFromAMissingChild(
        mutation: StoredItemMutation,
        childExists: Bool
    ) async throws {
        let host = "\(UUID().uuidString.lowercased()).example.invalid"
        let account = Account(user: "login", id: "user-id", serverUrl: "https://" + host, password: "test")
        let domain = NSFileProviderDomain(identifier: .init(UUID().uuidString), displayName: "Lookup test")
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let manager = try FilesDatabaseManager(
            account: account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: domain.identifier,
            log: FileProviderLogMock()
        )
        let remote = NextcloudKit()
        let provider = FileProviderExtension(domain: domain, ncKit: remote)
        defer { provider.invalidate() }
        provider.ncAccount = account
        provider.dbManager = manager
        provider.ignoredFiles = IgnoredFilesMatcher(ignoreList: [], log: FileProviderLogMock())

        let identifier = NSFileProviderItemIdentifier("known-child")
        let child = DatabaseTestSuites.makeFile(
            ocId: identifier.rawValue,
            fileName: "report.pdf",
            serverUrl: account.davFilesUrl + "/uncached-parent",
            account: account
        )
        if childExists {
            try manager.insertForTesting(child)
            try #require(manager.parentItemIdentifierFromMetadata(child) == nil)
        }

        let parentURL = try #require(URL(string: child.serverUrl))
        let childURL = try #require(URL(string: child.remotePath()))
        let capabilitiesURL = try #require(URL(string: account.serverUrl + "/ocs/v1.php/cloud/capabilities"))
        let capabilities = Data(#"{"ocs":{"meta":{"status":"ok","statuscode":100,"message":"OK"},"data":{"version":{"string":"32.0.5","minor":0,"micro":5,"major":32},"capabilities":{}}}}"#.utf8)
        MockHTTPURLProtocol.setResponse(for: capabilitiesURL, statusCode: 200, body: capabilities)
        MockHTTPURLProtocol.setResponse(for: parentURL, statusCode: 503, body: Data())
        MockHTTPURLProtocol.setResponse(for: childURL, statusCode: 204, body: Data())
        defer {
            MockHTTPURLProtocol.removeResponse(for: capabilitiesURL)
            MockHTTPURLProtocol.removeResponse(for: parentURL)
            MockHTTPURLProtocol.removeResponse(for: childURL)
        }

        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MockHTTPURLProtocol.self]
        let common = remote.nkCommonInstance
        let session = NKSession(
            nkCommonInstance: common,
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

        let error: (any Error)? = await withCheckedContinuation { continuation in
            switch mutation {
                case .modify:
                    let renamedItem = MockFileProviderItem(identifier: identifier, filename: "renamed.pdf", isUploaded: true)
                    _ = provider.modifyItem(
                        renamedItem,
                        baseVersion: NSFileProviderItemVersion(),
                        changedFields: [.filename],
                        contents: nil,
                        request: NSFileProviderRequest()
                    ) { _, _, _, error in
                        continuation.resume(returning: error)
                    }
                case .delete:
                    _ = provider.deleteItem(
                        identifier: identifier,
                        baseVersion: NSFileProviderItemVersion(),
                        request: NSFileProviderRequest()
                    ) { error in
                        continuation.resume(returning: error)
                    }
            }
        }

        let reportedError = try #require(error as NSError?)
        #expect(MockHTTPURLProtocol.requests(for: childURL).isEmpty)
        if childExists {
            #expect(reportedError.domain == NSCocoaErrorDomain)
            #expect(reportedError.code == CocoaError.Code.xpcConnectionReplyInvalid.rawValue)
            let underlyingError = reportedError.userInfo[NSUnderlyingErrorKey] as? NSError
            #expect(underlyingError?.domain == NSFileProviderErrorDomain)
            #expect(underlyingError?.code == NSFileProviderError.cannotSynchronize.rawValue)
            #expect(MockHTTPURLProtocol.requests(for: parentURL).map(\.httpMethod) == ["PROPFIND"])
            let stored = try #require(manager.itemMetadata(identifier))
            #expect(stored.remotePath() == child.remotePath())
            #expect(!stored.deleted)
        } else {
            #expect(reportedError.domain == NSFileProviderErrorDomain)
            #expect(reportedError.code == NSFileProviderError.noSuchItem.rawValue)
            #expect(MockHTTPURLProtocol.requests(for: parentURL).isEmpty)
            #expect(manager.itemMetadata(identifier) == nil)
        }
    }
}
