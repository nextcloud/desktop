// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import RealmSwift
import Testing

@Suite(.serialized)
struct ThumbnailFetchingTests {
    private static let account = Account(
        user: "thumbnail-user", id: "thumbnail-user", serverUrl: "https://example.invalid", password: "password"
    )

    private func makeExtension() -> FileProviderExtension {
        let domain = NSFileProviderDomain(identifier: .init(UUID().uuidString), displayName: "Thumbnail test")
        return FileProviderExtension(domain: domain)
    }

    @Test(arguments: [false, true])
    func forwardsDomainAndCallbacks(serverError: Bool) async throws {
        let ext = makeExtension()
        defer { ext.invalidate() }
        let previousConfiguration = Realm.Configuration.defaultConfiguration
        defer { Realm.Configuration.defaultConfiguration = previousConfiguration }
        let databaseDirectory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: databaseDirectory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: databaseDirectory) }
        let database = FilesDatabaseManager(
            account: Self.account,
            databaseDirectory: databaseDirectory,
            fileProviderDomainIdentifier: ext.domain.identifier,
            log: FileProviderLogMock()
        )
        ext.ncAccount = Self.account
        ext.dbManager = database
        let identifiers = [NSFileProviderItemIdentifier("thumbnail-item")]
        let size = CGSize(width: 64, height: 64)
        let data = serverError ? nil : Data("Thumbnail contents".utf8)
        let expectedError: Error? = serverError ? NSFileProviderError(.serverUnreachable) : nil
        let expectedProgress = Progress(totalUnitCount: 1)
        let expectedDomain = ext.domain
        let expectedRemoteInterface = ext.ncKit
        let (thumbnails, thumbnailContinuation) = AsyncStream<(NSFileProviderItemIdentifier, Data?, Error?)>.makeStream()
        let (completion, completionContinuation) = AsyncStream<Error?>.makeStream()
        defer {
            thumbnailContinuation.finish()
            completionContinuation.finish()
        }
        ext.thumbnailFetcher = { receivedIdentifiers, receivedSize, account, remoteInterface, dbManager, domain, perThumbnail, _, completed in
            #expect(receivedIdentifiers == identifiers)
            #expect(receivedSize == size)
            #expect(account == Self.account)
            #expect(ObjectIdentifier(remoteInterface as AnyObject) == ObjectIdentifier(expectedRemoteInterface))
            #expect(dbManager === database)
            #expect(domain === expectedDomain)
            perThumbnail(identifiers[0], data, expectedError)
            completed(expectedError)
            return expectedProgress
        }

        let progress = ext.fetchThumbnails(
            for: identifiers,
            requestedSize: size,
            perThumbnailCompletionHandler: { thumbnailContinuation.yield(($0, $1, $2)) },
            completionHandler: { completionContinuation.yield($0) }
        )

        #expect(progress === expectedProgress)
        var thumbnailIterator = thumbnails.makeAsyncIterator()
        let thumbnail = try #require(await thumbnailIterator.next())
        #expect(thumbnail.0 == identifiers[0])
        #expect(thumbnail.1 == data)
        #expect((thumbnail.2 as? NSFileProviderError)?.code == (expectedError as? NSFileProviderError)?.code)
        var completionIterator = completion.makeAsyncIterator()
        let error: Error? = try #require(await completionIterator.next())
        #expect((error as? NSFileProviderError)?.code == (expectedError as? NSFileProviderError)?.code)
    }

    @Test(arguments: [false, true])
    func unavailableSetupDoesNotStartThumbnailBatch(accountReady: Bool) async throws {
        let ext = makeExtension()
        defer { ext.invalidate() }
        if accountReady {
            ext.ncAccount = Self.account
        }
        ext.thumbnailFetcher = { _, _, _, _, _, _, _, _, _ in
            Issue.record("Unavailable account or database must not start a thumbnail batch")
            return Progress()
        }
        let (completion, continuation) = AsyncStream<Error?>.makeStream()
        defer { continuation.finish() }

        _ = ext.fetchThumbnails(
            for: [.init("thumbnail-item")],
            requestedSize: CGSize(width: 64, height: 64),
            perThumbnailCompletionHandler: { _, _, _ in Issue.record("No thumbnail batch was started") },
            completionHandler: { continuation.yield($0) }
        )

        var iterator = completion.makeAsyncIterator()
        let error: Error? = try #require(await iterator.next())
        #expect((error as? NSFileProviderError)?.code == (accountReady ? .cannotSynchronize : .notAuthenticated))
    }
}
