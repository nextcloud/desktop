//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import Testing
import TestInterface

@Suite("Exclude list in the root container")
struct RootContainerExcludeListTests {
    private let account = DatabaseTestSuites.account
    private let dbManager = DatabaseTestSuites.makeManager()
    private let rootItem: MockRemoteItem
    private let remoteInterface: MockRemoteInterface
    private let globalIgnoredFiles = IgnoredFilesMatcher(ignoreList: ["*.part"], log: FileProviderLogMock())

    init() {
        rootItem = MockRemoteItem.rootItem(account: account)
        remoteInterface = MockRemoteInterface(account: account, rootItem: rootItem)
    }

    ///
    /// Put an exclude list file into a folder on the server and into the local database, as enumeration would.
    ///
    /// - Parameters:
    ///     - parent: The folder to put the file into, or the root container if `nil`.
    ///
    @discardableResult
    private func addExcludeList(_ contents: String, etag: String, parent: MockRemoteItem? = nil) -> MockRemoteItem {
        let parent = parent ?? rootItem
        let excludeList = MockRemoteItem(
            identifier: "exclude-list-" + parent.identifier,
            versionIdentifier: etag,
            name: ".sync-exclude.lst",
            remotePath: parent.remotePath + "/.sync-exclude.lst",
            data: Data(contents.utf8),
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        parent.children.append(excludeList)
        excludeList.parent = parent
        dbManager.addItemMetadata(excludeList.toItemMetadata(account: account))

        return excludeList
    }

    private func addFolder() -> MockRemoteItem {
        let folder = MockRemoteItem(
            identifier: "folder",
            name: "folder",
            remotePath: account.davFilesUrl + "/folder",
            directory: true,
            account: account.ncKitAccount,
            username: account.username,
            userId: account.id,
            serverUrl: account.serverUrl
        )
        rootItem.children.append(folder)
        folder.parent = rootItem
        dbManager.addItemMetadata(folder.toItemMetadata(account: account))

        return folder
    }

    private func resolveMatcher(_ excludeList: RootContainerExcludeList) async throws -> IgnoredFilesMatcher {
        try await excludeList.matcher(extending: globalIgnoredFiles, account: account, remoteInterface: remoteInterface, dbManager: dbManager)
    }

    @Test func withoutExcludeListOnlyGlobalPatternsApply() async throws {
        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        #expect(matcher === globalIgnoredFiles)
        #expect(remoteInterface.downloadOperationCount == 0)
    }

    @Test func excludeListAppliesToTheWholeDomainWithGlobalPatterns() async throws {
        addExcludeList("*.excl\nfoo/bar\nbuild/\n", etag: "1")

        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        #expect(matcher.isExcluded("a.excl"))
        #expect(matcher.isExcluded("deep/nested/a.excl"))
        #expect(matcher.isExcluded("foo/bar/file.txt"))
        #expect(!matcher.isExcluded("other/foo/bar"))
        #expect(matcher.isExcluded("sub/build", isDirectory: true))
        #expect(matcher.isExcluded("sub/c.part"))
        #expect(!matcher.isExcluded("a.txt"))
        #expect(!matcher.isExcluded(".sync-exclude.lst"))
    }

    @Test func excludeListInSubfolderIsNotRead() async throws {
        let folder = addFolder()
        addExcludeList("*.excl\n", etag: "1", parent: folder)

        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        #expect(!matcher.isExcluded("folder/a.excl"))
        #expect(remoteInterface.downloadOperationCount == 0)
    }

    @Test func deletedExcludeListIsIgnored() async throws {
        let excludeList = addExcludeList("*.excl\n", etag: "1")
        var metadata = excludeList.toItemMetadata(account: account)
        metadata.deleted = true
        dbManager.addItemMetadata(metadata)

        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        #expect(!matcher.isExcluded("a.excl"))
        #expect(remoteInterface.downloadOperationCount == 0)
    }

    @Test func excludeListIsDownloadedAgainOnlyWhenItsEtagChanges() async throws {
        let excludeList = addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())

        var matcher = try await resolveMatcher(rootContainerExcludeList)
        #expect(matcher.isExcluded("a.excl"))
        matcher = try await resolveMatcher(rootContainerExcludeList)
        #expect(matcher.isExcluded("a.excl"))
        #expect(remoteInterface.downloadOperationCount == 1)

        excludeList.data = Data("*.other\n".utf8)
        excludeList.versionIdentifier = "2"
        dbManager.addItemMetadata(excludeList.toItemMetadata(account: account))

        matcher = try await resolveMatcher(rootContainerExcludeList)
        #expect(!matcher.isExcluded("a.excl"))
        #expect(matcher.isExcluded("a.other"))
        #expect(remoteInterface.downloadOperationCount == 2)
    }

    @Test func uploadedExcludeListIsUsedWithoutDownload() async throws {
        let excludeList = addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())
        let contents = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data("*.uploaded\n".utf8).write(to: contents)
        defer { try? FileManager.default.removeItem(at: contents) }
        excludeList.versionIdentifier = "2"
        let uploadedMetadata = excludeList.toItemMetadata(account: account)

        await rootContainerExcludeList.didUpload(uploadedMetadata, contents: contents, account: account)
        dbManager.addItemMetadata(uploadedMetadata)
        let matcher = try await resolveMatcher(rootContainerExcludeList)

        #expect(matcher.isExcluded("a.uploaded"))
        #expect(!matcher.isExcluded("a.excl"))
        #expect(remoteInterface.downloadOperationCount == 0)
    }

    @Test func uploadOfOtherItemsDoesNotChangeExcludeList() async throws {
        addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())
        let contents = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data("*.other\n".utf8).write(to: contents)
        defer { try? FileManager.default.removeItem(at: contents) }
        var otherMetadata = SendableItemMetadata(ocId: "other", fileName: ".sync-exclude.lst", account: account)
        otherMetadata.serverUrl = account.davFilesUrl + "/folder"
        otherMetadata.etag = "1"

        await rootContainerExcludeList.didUpload(otherMetadata, contents: contents, account: account)
        let matcher = try await resolveMatcher(rootContainerExcludeList)

        #expect(matcher.isExcluded("a.excl"))
        #expect(!matcher.isExcluded("a.other"))
        #expect(remoteInterface.downloadOperationCount == 1)
    }

    @Test(arguments: [
        (503, NSFileProviderError.Code.serverUnreachable),
        (401, NSFileProviderError.Code.notAuthenticated),
        (403, NSFileProviderError.Code.cannotSynchronize)
    ])
    func failsWithRetryableErrorWhenExcludeListCannotBeDownloaded(statusCode: Int, expectedCode: NSFileProviderError.Code) async throws {
        addExcludeList("*.excl\n", etag: "1")
        remoteInterface.downloadError = NKError(statusCode: statusCode, fallbackDescription: "Download failed")

        let error = await #expect(throws: NSFileProviderError.self) {
            try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))
        }

        #expect(error?.code == expectedCode)
    }

    @Test func excludeListMissingOnServerExcludesNothing() async throws {
        addExcludeList("*.excl\n", etag: "1")
        remoteInterface.downloadError = NKError(statusCode: 404, fallbackDescription: "Not Found")

        let matcher = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        #expect(!matcher.isExcluded("a.excl"))
        #expect(matcher.isExcluded("a.part"))
    }

    @Test func usesEarlierVersionWhenCurrentCannotBeDownloaded() async throws {
        let excludeList = addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())
        _ = try await resolveMatcher(rootContainerExcludeList)

        excludeList.versionIdentifier = "2"
        dbManager.addItemMetadata(excludeList.toItemMetadata(account: account))
        remoteInterface.downloadError = NKError(statusCode: 503, fallbackDescription: "Service Unavailable")
        let matcher = try await resolveMatcher(rootContainerExcludeList)

        #expect(matcher.isExcluded("a.excl"))
        #expect(remoteInterface.downloadOperationCount == 2)
    }

    @Test func concurrentCallersShareOneDownload() async throws {
        addExcludeList("*.excl\n", etag: "1")
        let rootContainerExcludeList = RootContainerExcludeList(log: FileProviderLogMock())
        // Keep the download in progress long enough for every caller to arrive.
        remoteInterface.downloadHandler = {
            try? await Task.sleep(nanoseconds: 200_000_000)
        }
        let account = account
        let ignoredFiles = globalIgnoredFiles
        let remoteInterface = remoteInterface
        let dbManager = dbManager

        let results = try await withThrowingTaskGroup(of: Bool.self) { group in
            for _ in 0 ..< 20 {
                group.addTask {
                    try await rootContainerExcludeList
                        .matcher(extending: ignoredFiles, account: account, remoteInterface: remoteInterface, dbManager: dbManager)
                        .isExcluded("a.excl")
                }
            }

            return try await group.reduce(into: [Bool]()) { $0.append($1) }
        }

        #expect(results == Array(repeating: true, count: 20))
        #expect(remoteInterface.downloadOperationCount == 1)
    }

    @Test func createInSubfolderIsNotUploadedWhenExcludedByExcludeList() async throws {
        addExcludeList("*.excl\n", etag: "1")
        let folder = addFolder()
        var metadata = SendableItemMetadata(ocId: "excluded-file", fileName: "a.excl", account: account)
        metadata.serverUrl = folder.remotePath
        let itemTemplate = Item(
            metadata: metadata,
            parentItemIdentifier: .init(folder.identifier),
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager
        )
        let contents = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data("Hello world".utf8).write(to: contents)
        defer { try? FileManager.default.removeItem(at: contents) }
        let ignoredFiles = try await resolveMatcher(RootContainerExcludeList(log: FileProviderLogMock()))

        let (createdItem, error) = await Item.create(
            basedOn: itemTemplate,
            contents: contents,
            account: account,
            remoteInterface: remoteInterface,
            ignoredFiles: ignoredFiles,
            progress: .init(),
            dbManager: dbManager,
            log: FileProviderLogMock()
        )

        #expect(error as? NSFileProviderError == NSFileProviderError(.excludedFromSync))
        #expect(createdItem?.isUploaded == false)
        #expect(folder.children.isEmpty)
    }
}
