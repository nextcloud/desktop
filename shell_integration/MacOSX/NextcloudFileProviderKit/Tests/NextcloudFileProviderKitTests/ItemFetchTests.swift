//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import TestInterface
import UniformTypeIdentifiers
import XCTest

final class ItemFetchTests: NextcloudFileProviderKitTestCase {
    private func removeDownloadedContents(remoteInterface: MockRemoteInterface) {
        guard let localPath = remoteInterface.downloadDestinationURL else { return }
        try? FileManager.default.removeItem(at: localPath)
    }

    private func makeFetchItem(directory: Bool = false, preview: Bool = false) -> (Item, MockRemoteInterface, MockRemoteItem) {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.injectMock(Self.account)
        let identifier = UUID().uuidString
        let name = directory ? identifier : identifier + ".txt"
        let remoteItem = MockRemoteItem(
            identifier: identifier,
            versionIdentifier: "0",
            name: name,
            remotePath: Self.account.davFilesUrl + "/" + name,
            directory: directory,
            data: directory ? nil : Data("Downloaded contents".utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children.append(remoteItem)
        remoteItem.parent = rootItem
        if directory {
            for name in ["first.txt", "second.txt", "third.txt"] {
                let child = MockRemoteItem(
                    identifier: UUID().uuidString,
                    name: name,
                    remotePath: remoteItem.remotePath + "/" + name,
                    data: Data(name.utf8),
                    account: Self.account.ncKitAccount,
                    username: Self.account.username,
                    userId: Self.account.id,
                    serverUrl: Self.account.serverUrl
                )
                child.parent = remoteItem
                remoteItem.children.append(child)
            }
        }
        var metadata = remoteItem.toItemMetadata(account: Self.account)
        metadata.downloaded = false
        metadata.hasPreview = preview
        Self.dbManager.addItemMetadata(metadata)
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        return (item, remoteInterface, remoteItem)
    }

    private func makeOverlappingFetchItems() -> (Item, MockRemoteInterface, Item, MockRemoteInterface, MockRemoteItem) {
        let (firstItem, firstRemoteInterface, remoteItem) = makeFetchItem()
        let secondRemoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        secondRemoteInterface.injectMock(Self.account)
        let secondDBManager = FilesDatabaseManager(
            realmConfiguration: Self.dbManager.ncDatabase().configuration,
            account: Self.account,
            databaseDirectory: Self.makeDatabaseDirectory(),
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            log: FileProviderLogMock()
        )
        let secondItem = Item(
            metadata: remoteItem.toItemMetadata(account: Self.account),
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: secondRemoteInterface,
            dbManager: secondDBManager
        )
        return (firstItem, firstRemoteInterface, secondItem, secondRemoteInterface, remoteItem)
    }

    private func startFetchWaitingForCancellation(
        item: Item,
        remoteInterface: MockRemoteInterface
    ) async throws -> (Task<(URL?, Item?, Error?), Never>, Progress) {
        await RetrievedCapabilitiesActor.shared.reset()
        let progress = Progress()
        let (started, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        remoteInterface.downloadCompletionHandler = {
            XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, true)
            continuation.yield(())
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation did not reach the operation.")
        }
        let dbManager = Self.dbManager
        let task = Task { await item.fetchContents(progress: progress, dbManager: dbManager) }
        do {
            guard try await nextTestValue(from: started) != nil else {
                throw URLError(.badServerResponse)
            }
            return (task, progress)
        } catch {
            progress.cancel()
            task.cancel()
            throw error
        }
    }

    private func startSuspendedFetch(
        item: Item,
        remoteInterface: MockRemoteInterface
    ) async throws -> (Task<(URL?, Item?, Error?), Never>, AsyncStream<Void>.Continuation) {
        let (started, startedContinuation) = AsyncStream<Void>.makeStream()
        let (release, releaseContinuation) = AsyncStream<Void>.makeStream()
        defer { startedContinuation.finish() }
        remoteInterface.downloadHandler = {
            startedContinuation.yield(())
            do {
                _ = try await nextTestValue(from: release)
            } catch {
                XCTFail("Suspended download was not released: \(error)")
            }
        }
        let dbManager = item.dbManager
        let task = Task { await item.fetchContents(dbManager: dbManager) }
        do {
            guard try await nextTestValue(from: started) != nil else {
                throw URLError(.badServerResponse)
            }
            return (task, releaseContinuation)
        } catch {
            releaseContinuation.finish()
            task.cancel()
            throw error
        }
    }

    private func makeNestedFetchItem() -> (Item, MockRemoteInterface, MockRemoteItem) {
        let (_, remoteInterface, directory) = makeFetchItem(directory: true)
        let child = directory.children[0]
        var metadata = child.toItemMetadata(account: Self.account)
        metadata.hasPreview = true
        Self.dbManager.addItemMetadata(metadata)
        Self.dbManager.removeItemMetadata(ocId: directory.identifier)
        let item = Item(
            metadata: metadata,
            parentItemIdentifier: NSFileProviderItemIdentifier(directory.identifier),
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )
        return (item, remoteInterface, directory)
    }

    private func makeThumbnailBatch(
        identifiers: [NSFileProviderItemIdentifier],
        remoteInterface: MockRemoteInterface
    ) -> (
        progress: Progress,
        thumbnails: AsyncStream<(identifier: NSFileProviderItemIdentifier, data: Data?, error: Error?)>,
        completion: AsyncStream<Error?>
    ) {
        let (thumbnails, thumbnailContinuation) = AsyncStream<(identifier: NSFileProviderItemIdentifier, data: Data?, error: Error?)>.makeStream()
        let (completion, completionContinuation) = AsyncStream<Error?>.makeStream()
        let progress = NextcloudFileProviderKit.fetchThumbnails(
            for: identifiers,
            requestedSize: CGSize(width: 32, height: 32),
            account: Self.account,
            usingRemoteInterface: remoteInterface,
            andDatabase: Self.dbManager,
            perThumbnailCompletionHandler: { identifier, data, error in
                thumbnailContinuation.yield((identifier, data, error))
            },
            log: FileProviderLogMock(),
            completionHandler: { error in
                completionContinuation.yield(error)
                completionContinuation.finish()
                thumbnailContinuation.finish()
            }
        )
        return (progress, thumbnails, completion)
    }

    static let account = Account(
        user: "testUser", id: "testUserId", serverUrl: "https://mock.nc.com", password: "abcd"
    )

    lazy var rootItem = MockRemoteItem.rootItem(account: Self.account)
    static let dbManager = FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        setUpDatabase(Self.dbManager)
    }

    override func tearDown() {
        rootItem.children = []
        super.tearDown()
    }

    func testFetchFixturesRemainLiveAtDistinctRemoteLocations() throws {
        for directory in [false, true] {
            let (first, _, firstRemoteItem) = makeFetchItem(directory: directory)
            let (second, _, secondRemoteItem) = makeFetchItem(directory: directory)

            XCTAssertNotEqual(firstRemoteItem.remotePath, secondRemoteItem.remotePath)
            XCTAssertFalse(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: first.itemIdentifier.rawValue)).deleted)
            XCTAssertFalse(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: second.itemIdentifier.rawValue)).deleted)
            XCTAssertTrue(rootItem.children.contains { $0.identifier == firstRemoteItem.identifier })
            XCTAssertTrue(rootItem.children.contains { $0.identifier == secondRemoteItem.identifier })
        }
    }

    func testCancelledProgressDoesNotStartDownload() async throws {
        let (item, remoteInterface, _) = makeFetchItem()
        let progress = Progress()
        progress.cancel()

        let (url, fetchedItem, error) = await item.fetchContents(progress: progress, dbManager: Self.dbManager)

        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        XCTAssertEqual(remoteInterface.downloadOperationCount, 0)
        XCTAssertFalse(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)).downloaded)
    }

    func testCancelledDownloadAllowsLaterServerChanges() async throws {
        for directory in [false, true] {
            for cancelProgress in [false, true] {
                let (item, remoteInterface, remoteItem) = makeFetchItem(directory: directory)
                let progress = Progress()
                defer {
                    removeDownloadedContents(remoteInterface: remoteInterface)
                    if directory {
                        try? FileManager.default.removeItem(at: FileManager.default.temporaryDirectory.appendingPathComponent(remoteItem.identifier))
                    }
                }
                if cancelProgress {
                    remoteInterface.downloadHandler = { progress.cancel() }
                } else {
                    remoteInterface.downloadError = NKError(errorCode: NSURLErrorCancelled, errorDescription: "Download cancelled")
                }
                let (url, fetchedItem, error) = await item.fetchContents(progress: progress, dbManager: Self.dbManager)
                XCTAssertNil(url)
                XCTAssertNil(fetchedItem)
                XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
                let cancelledMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteItem.identifier))

                let oldPath = remoteItem.remotePath
                remoteItem.name = "renamed-" + remoteItem.name
                remoteItem.remotePath = Self.account.davFilesUrl + "/" + remoteItem.name
                for child in remoteItem.children {
                    child.remotePath = child.remotePath.replacingOccurrences(of: oldPath, with: remoteItem.remotePath)
                }
                let rename = await Enumerator.readServerUrl(Self.account.davFilesUrl, account: Self.account, remoteInterface: remoteInterface, dbManager: Self.dbManager, log: FileProviderLogMock())
                XCTAssertNil(rename.error)
                XCTAssertTrue(try XCTUnwrap(rename.changes).updated.contains { $0.ocId == remoteItem.identifier })
                XCTAssertEqual(Self.dbManager.itemMetadata(ocId: remoteItem.identifier)?.fileName, remoteItem.name)

                remoteItem.versionIdentifier = "updated-content"
                remoteItem.data = directory ? nil : Data("New server contents".utf8)
                let contents = await Enumerator.readServerUrl(directory ? remoteItem.remotePath : Self.account.davFilesUrl, account: Self.account, remoteInterface: remoteInterface, dbManager: Self.dbManager, log: FileProviderLogMock())
                XCTAssertNil(contents.error)
                XCTAssertTrue(try XCTUnwrap(contents.changes).updated.contains { $0.ocId == remoteItem.identifier })
                let updatedMetadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteItem.identifier))
                XCTAssertEqual(updatedMetadata.etag, remoteItem.versionIdentifier)
                XCTAssertFalse(updatedMetadata.downloaded)
                XCTAssertEqual(updatedMetadata.status, Status.normal.rawValue)

                XCTAssertEqual(cancelledMetadata.status, Status.normal.rawValue)
                XCTAssertFalse(cancelledMetadata.downloaded)
                XCTAssertEqual(cancelledMetadata.sessionError, "")
                let cancelledItem = Item(metadata: cancelledMetadata, parentItemIdentifier: .rootContainer, account: Self.account, remoteInterface: remoteInterface, dbManager: Self.dbManager)
                XCTAssertNil(cancelledItem.downloadingError)
            }
        }
    }

    func testCancellingDownloadCancelsNetworkHandlesAndAllowsRetry() async throws {
        let (item, remoteInterface, remoteItem) = makeFetchItem()
        let progress = Progress()
        let session = Session(startRequestsImmediately: false)
        let request = session.download("https://example.invalid/resource")
        let urlSession = URLSession(configuration: .ephemeral)
        defer { urlSession.invalidateAndCancel() }
        let networkTask = try urlSession.dataTask(with: XCTUnwrap(URL(string: "https://example.invalid/resource")))
        remoteInterface.downloadRequest = request
        remoteInterface.downloadTask = networkTask
        remoteInterface.downloadHandler = {
            progress.cancel()
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation did not reach the operation.")
        }

        let (url, fetchedItem, error) = await item.fetchContents(progress: progress, dbManager: Self.dbManager)

        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        XCTAssertTrue(request.isCancelled)
        XCTAssertTrue(networkTask.state == .canceling || networkTask.state == .completed)
        let metadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue))
        XCTAssertFalse(metadata.downloaded)
        XCTAssertEqual(metadata.status, Status.normal.rawValue)
        XCTAssertEqual(metadata.sessionError, "")
        XCTAssertNil(progress.cancellationHandler)
        XCTAssertNil(progress.pausingHandler)
        XCTAssertNil(progress.resumingHandler)

        remoteInterface.downloadRequest = nil
        remoteInterface.downloadTask = nil
        remoteInterface.downloadHandler = nil
        let (retryUrl, retryItem, retryError) = await item.fetchContents(dbManager: Self.dbManager)
        XCTAssertNil(retryError)
        let downloadedUrl = try XCTUnwrap(retryUrl)
        defer { try? FileManager.default.removeItem(at: downloadedUrl) }
        XCTAssertEqual(try Data(contentsOf: downloadedUrl), remoteItem.data)
        XCTAssertTrue(try XCTUnwrap(retryItem).isDownloaded)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.status, Status.normal.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.sessionError, "")
    }

    func testParentCancellationReachesDownload() async throws {
        let (item, remoteInterface, _) = makeFetchItem()
        let dbManager = Self.dbManager
        let (started, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        remoteInterface.downloadHandler = {
            continuation.yield(())
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation did not reach the operation.")
        }
        let task = Task { await item.fetchContents(dbManager: dbManager) }
        defer { task.cancel() }
        guard let _ = try await nextTestValue(from: started) else {
            throw URLError(.badServerResponse)
        }
        task.cancel()

        let (url, fetchedItem, error) = try await testTaskValue(of: task)
        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.status, Status.normal.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.sessionError, "")
    }

    func testCancelledSuccessfulResponseDoesNotMarkContentsDownloaded() async throws {
        let (item, remoteInterface, _) = makeFetchItem()
        let progress = Progress()
        defer { removeDownloadedContents(remoteInterface: remoteInterface) }
        remoteInterface.downloadCompletionHandler = {
            XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, true)
            progress.cancel()
        }

        let (url, fetchedItem, error) = await item.fetchContents(progress: progress, dbManager: Self.dbManager)

        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        let metadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue))
        XCTAssertFalse(metadata.downloaded)
        XCTAssertEqual(metadata.status, Status.normal.rawValue)
        XCTAssertEqual(metadata.sessionError, "")
        XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, false)
    }

    func testCancellationAfterSuccessfulFetchPreservesReturnedContents() async throws {
        let (item, _, remoteItem) = makeFetchItem()
        let progress = Progress()

        let (url, fetchedItem, error) = await item.fetchContents(progress: progress, dbManager: Self.dbManager)
        let localPath = try XCTUnwrap(url)
        defer { try? FileManager.default.removeItem(at: localPath) }
        progress.cancel()

        XCTAssertNil(error)
        XCTAssertTrue(try XCTUnwrap(fetchedItem).isDownloaded)
        XCTAssertEqual(try Data(contentsOf: localPath), remoteItem.data)
        let metadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue))
        XCTAssertTrue(metadata.downloaded)
        XCTAssertEqual(metadata.status, Status.normal.rawValue)
    }

    func testCancellingOverlappingFetchPreservesReturnedContents() async throws {
        let (firstItem, firstRemoteInterface, secondItem, secondRemoteInterface, remoteItem) = makeOverlappingFetchItems()
        let dbManager = Self.dbManager
        let secondDBManager = secondItem.dbManager
        defer {
            removeDownloadedContents(remoteInterface: firstRemoteInterface)
            removeDownloadedContents(remoteInterface: secondRemoteInterface)
        }
        let (firstTask, firstProgress) = try await startFetchWaitingForCancellation(item: firstItem, remoteInterface: firstRemoteInterface)
        defer { firstProgress.cancel() }

        let (secondURL, secondFetchedItem, secondError) = await secondItem.fetchContents(dbManager: secondDBManager)
        XCTAssertNil(secondError)
        XCTAssertTrue(try XCTUnwrap(secondFetchedItem).isDownloaded)
        let returnedURL = try XCTUnwrap(secondURL)
        XCTAssertEqual(try Data(contentsOf: returnedURL), remoteItem.data)
        var metadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertTrue(metadata.downloaded)
        XCTAssertEqual(metadata.status, Status.normal.rawValue)
        XCTAssertEqual(metadata.sessionError, "")
        metadata.favorite = true
        dbManager.addItemMetadata(metadata)

        firstProgress.cancel()
        let (firstURL, firstFetchedItem, firstError) = try await testTaskValue(of: firstTask)

        XCTAssertNil(firstURL)
        XCTAssertNil(firstFetchedItem)
        XCTAssertEqual((firstError as? CocoaError)?.code, .userCancelled)
        XCTAssertTrue(FileManager.default.fileExists(atPath: returnedURL.path))
        XCTAssertEqual(try Data(contentsOf: returnedURL), remoteItem.data)
        XCTAssertEqual(firstRemoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, false)
        let remainingMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertTrue(remainingMetadata.downloaded)
        XCTAssertEqual(remainingMetadata.status, Status.normal.rawValue)
        XCTAssertEqual(remainingMetadata.sessionError, "")
        XCTAssertTrue(remainingMetadata.favorite)
    }

    func testCancellingOlderFetchPreservesNewerDownloadInProgress() async throws {
        let (firstItem, firstRemoteInterface, secondItem, secondRemoteInterface, remoteItem) = makeOverlappingFetchItems()
        let dbManager = Self.dbManager
        let secondDBManager = secondItem.dbManager
        defer {
            removeDownloadedContents(remoteInterface: firstRemoteInterface)
            removeDownloadedContents(remoteInterface: secondRemoteInterface)
        }
        let (firstTask, firstProgress) = try await startFetchWaitingForCancellation(item: firstItem, remoteInterface: firstRemoteInterface)
        defer { firstProgress.cancel() }
        let (started, startedContinuation) = AsyncStream<Void>.makeStream()
        let (release, releaseContinuation) = AsyncStream<Void>.makeStream()
        defer {
            startedContinuation.finish()
            releaseContinuation.finish()
        }
        secondRemoteInterface.downloadHandler = {
            startedContinuation.yield(())
            do {
                _ = try await nextTestValue(from: release)
            } catch {
                XCTFail("Suspended download was not released: \(error)")
            }
        }
        let secondTask = Task { await secondItem.fetchContents(dbManager: secondDBManager) }
        defer { secondTask.cancel() }
        guard let _ = try await nextTestValue(from: started) else {
            throw URLError(.badServerResponse)
        }
        let downloadingMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertFalse(downloadingMetadata.downloaded)
        XCTAssertEqual(downloadingMetadata.status, Status.downloading.rawValue)

        firstProgress.cancel()
        let (firstURL, firstFetchedItem, firstError) = try await testTaskValue(of: firstTask)

        XCTAssertNil(firstURL)
        XCTAssertNil(firstFetchedItem)
        XCTAssertEqual((firstError as? CocoaError)?.code, .userCancelled)
        let remainingMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertFalse(remainingMetadata.downloaded)
        XCTAssertEqual(remainingMetadata.status, Status.downloading.rawValue)
        XCTAssertEqual(remainingMetadata.sessionError, downloadingMetadata.sessionError)
        releaseContinuation.yield(())
        let (secondURL, secondFetchedItem, secondError) = try await testTaskValue(of: secondTask)
        XCTAssertNil(secondError)
        XCTAssertTrue(try XCTUnwrap(secondFetchedItem).isDownloaded)
        XCTAssertEqual(try Data(contentsOf: XCTUnwrap(secondURL)), remoteItem.data)
        XCTAssertEqual(dbManager.itemMetadata(ocId: remoteItem.identifier)?.status, Status.normal.rawValue)
    }

    func testCancellingOlderFetchPreservesNewerDownloadError() async throws {
        let (firstItem, firstRemoteInterface, secondItem, secondRemoteInterface, remoteItem) = makeOverlappingFetchItems()
        let dbManager = Self.dbManager
        let secondDBManager = secondItem.dbManager
        defer {
            removeDownloadedContents(remoteInterface: firstRemoteInterface)
            removeDownloadedContents(remoteInterface: secondRemoteInterface)
        }
        let (firstTask, firstProgress) = try await startFetchWaitingForCancellation(item: firstItem, remoteInterface: firstRemoteInterface)
        defer { firstProgress.cancel() }
        let downloadError = NKError(errorCode: 503, errorDescription: "Newer download failed")
        secondRemoteInterface.downloadError = downloadError
        let (secondURL, secondFetchedItem, secondError) = await secondItem.fetchContents(dbManager: secondDBManager)
        XCTAssertNil(secondURL)
        XCTAssertNil(secondFetchedItem)
        XCTAssertEqual((secondError as? NSFileProviderError)?.code, .serverUnreachable)
        let failedMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertFalse(failedMetadata.downloaded)
        XCTAssertEqual(failedMetadata.status, Status.downloadError.rawValue)
        XCTAssertEqual(failedMetadata.sessionError, downloadError.errorDescription)

        firstProgress.cancel()
        let (firstURL, firstFetchedItem, firstError) = try await testTaskValue(of: firstTask)

        XCTAssertNil(firstURL)
        XCTAssertNil(firstFetchedItem)
        XCTAssertEqual((firstError as? CocoaError)?.code, .userCancelled)
        let remainingMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertFalse(remainingMetadata.downloaded)
        XCTAssertEqual(remainingMetadata.status, Status.downloadError.rawValue)
        XCTAssertEqual(remainingMetadata.sessionError, downloadError.errorDescription)
    }

    func testCancellingFetchDoesNotRestoreRemovedMetadata() async throws {
        let (item, remoteInterface, remoteItem) = makeFetchItem()
        let dbManager = Self.dbManager
        defer { removeDownloadedContents(remoteInterface: remoteInterface) }
        let (task, progress) = try await startFetchWaitingForCancellation(item: item, remoteInterface: remoteInterface)
        defer { progress.cancel() }
        dbManager.removeItemMetadata(ocId: remoteItem.identifier)
        XCTAssertNil(dbManager.itemMetadata(ocId: remoteItem.identifier))

        progress.cancel()
        let (url, fetchedItem, error) = try await testTaskValue(of: task)

        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        XCTAssertNil(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, false)
    }

    func testSupersededSuccessfulFetchPreservesNewerCompletedFetch() async throws {
        let errorCodes: [Int?] = [nil, 503, NSURLErrorCancelled]
        for errorCode in errorCodes {
            let (firstItem, firstRemoteInterface, secondItem, secondRemoteInterface, remoteItem) = makeOverlappingFetchItems()
            let dbManager = Self.dbManager
            let (firstTask, releaseFirst) = try await startSuspendedFetch(item: firstItem, remoteInterface: firstRemoteInterface)
            defer {
                firstTask.cancel()
                releaseFirst.finish()
                removeDownloadedContents(remoteInterface: firstRemoteInterface)
                removeDownloadedContents(remoteInterface: secondRemoteInterface)
            }
            let secondProgress = Progress()
            if errorCode == NSURLErrorCancelled {
                secondRemoteInterface.downloadHandler = {
                    secondProgress.cancel()
                    let cancelled = await waitForCancellation()
                    XCTAssertTrue(cancelled, "Cancellation did not reach the operation.")
                }
            } else if let errorCode {
                secondRemoteInterface.downloadError = NKError(errorCode: errorCode, errorDescription: "Newer download failed")
            }
            let (secondURL, secondFetchedItem, secondError) = await secondItem.fetchContents(progress: secondProgress, dbManager: secondItem.dbManager)
            if let errorCode {
                XCTAssertNil(secondURL)
                XCTAssertNil(secondFetchedItem)
                if errorCode == NSURLErrorCancelled {
                    XCTAssertEqual((secondError as? CocoaError)?.code, .userCancelled)
                } else {
                    XCTAssertEqual((secondError as? NSFileProviderError)?.code, .serverUnreachable)
                }
            } else {
                XCTAssertNil(secondError)
                XCTAssertTrue(try XCTUnwrap(secondFetchedItem).isDownloaded)
                XCTAssertEqual(try Data(contentsOf: XCTUnwrap(secondURL)), remoteItem.data)
            }
            var newerMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
            newerMetadata.favorite = true
            dbManager.addItemMetadata(newerMetadata)

            releaseFirst.finish()
            let (firstURL, firstFetchedItem, firstError) = try await testTaskValue(of: firstTask)

            XCTAssertNil(firstURL)
            XCTAssertNil(firstFetchedItem)
            XCTAssertEqual((firstError as? CocoaError)?.code, .userCancelled)
            XCTAssertEqual(firstRemoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, false)
            let remainingMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
            XCTAssertEqual(remainingMetadata.downloaded, newerMetadata.downloaded)
            XCTAssertEqual(remainingMetadata.status, newerMetadata.status)
            XCTAssertEqual(remainingMetadata.sessionError, newerMetadata.sessionError)
            XCTAssertTrue(remainingMetadata.favorite)
            if let secondURL {
                XCTAssertEqual(try Data(contentsOf: secondURL), remoteItem.data)
            }
        }
    }

    func testSupersededSuccessfulFetchPreservesNewerDownloadInProgress() async throws {
        let (firstItem, firstRemoteInterface, secondItem, secondRemoteInterface, remoteItem) = makeOverlappingFetchItems()
        let dbManager = Self.dbManager
        let (firstTask, releaseFirst) = try await startSuspendedFetch(item: firstItem, remoteInterface: firstRemoteInterface)
        let (secondTask, releaseSecond) = try await startSuspendedFetch(item: secondItem, remoteInterface: secondRemoteInterface)
        defer {
            firstTask.cancel()
            secondTask.cancel()
            releaseFirst.finish()
            releaseSecond.finish()
            removeDownloadedContents(remoteInterface: firstRemoteInterface)
            removeDownloadedContents(remoteInterface: secondRemoteInterface)
        }
        let newerMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertFalse(newerMetadata.downloaded)
        XCTAssertEqual(newerMetadata.status, Status.downloading.rawValue)

        releaseFirst.finish()
        let (firstURL, firstFetchedItem, firstError) = try await testTaskValue(of: firstTask)

        XCTAssertNil(firstURL)
        XCTAssertNil(firstFetchedItem)
        XCTAssertEqual((firstError as? CocoaError)?.code, .userCancelled)
        XCTAssertEqual(firstRemoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, false)
        let remainingMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertFalse(remainingMetadata.downloaded)
        XCTAssertEqual(remainingMetadata.status, Status.downloading.rawValue)
        XCTAssertEqual(remainingMetadata.sessionError, newerMetadata.sessionError)
        releaseSecond.finish()
        let (secondURL, secondFetchedItem, secondError) = try await testTaskValue(of: secondTask)
        XCTAssertNil(secondError)
        XCTAssertTrue(try XCTUnwrap(secondFetchedItem).isDownloaded)
        XCTAssertEqual(try Data(contentsOf: XCTUnwrap(secondURL)), remoteItem.data)
        XCTAssertEqual(dbManager.itemMetadata(ocId: remoteItem.identifier)?.status, Status.normal.rawValue)
    }

    func testDirectoryFetchDownloadsChildRestoredByEnumeration() async throws {
        for downloadFails in [false, true] {
            let (item, remoteInterface, directory) = makeFetchItem(directory: true)
            let child = directory.children[0]
            var metadata = child.toNKFile().toItemMetadata()
            metadata.deleted = true
            metadata.keepDownloaded = true
            metadata.fileProviderContentVersion = "previous-content-version"
            Self.dbManager.addItemMetadata(metadata)
            let localDirectory = FileManager.default.temporaryDirectory.appendingPathComponent(directory.identifier)
            defer { try? FileManager.default.removeItem(at: localDirectory) }

            let read = await Enumerator.readServerUrl(directory.remotePath, account: Self.account, remoteInterface: remoteInterface, dbManager: Self.dbManager, log: FileProviderLogMock())
            XCTAssertNil(read.error)
            XCTAssertTrue(try XCTUnwrap(read.metadatas).contains { $0.ocId == child.identifier })
            XCTAssertFalse(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: child.identifier)).deleted)
            if downloadFails {
                remoteInterface.downloadError = NKError(errorCode: 503, errorDescription: "Child download failed")
            }

            let (url, fetchedItem, error) = await item.fetchContents(dbManager: Self.dbManager)
            if downloadFails {
                XCTAssertNil(url)
                XCTAssertNil(fetchedItem)
                XCTAssertEqual((error as? NSFileProviderError)?.code, .serverUnreachable)
            } else {
                XCTAssertNil(error)
                XCTAssertEqual(try XCTUnwrap(fetchedItem).itemIdentifier, item.itemIdentifier)
                let localPath = try XCTUnwrap(url)
                XCTAssertEqual(remoteInterface.downloadOperationCount, directory.children.count)
                for child in directory.children {
                    XCTAssertEqual(try Data(contentsOf: localPath.appendingPathComponent(child.name)), child.data)
                    XCTAssertTrue(try XCTUnwrap(Self.dbManager.itemMetadata(ocId: child.identifier)).downloaded)
                }
            }
            let restored = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: child.identifier))
            XCTAssertFalse(restored.deleted)
            XCTAssertTrue(restored.keepDownloaded)
            XCTAssertEqual(restored.fileProviderContentVersion, "previous-content-version")
            XCTAssertEqual(restored.downloaded, !downloadFails)
            XCTAssertEqual(restored.status, downloadFails ? Status.downloadError.rawValue : Status.normal.rawValue)
            XCTAssertEqual(restored.sessionError, downloadFails ? "Child download failed" : "")
        }
    }

    func testDirectoryFetchRejectsChildRemovedAfterEnumeration() async throws {
        for removeRow in [false, true] {
            let (item, remoteInterface, directory) = makeFetchItem(directory: true)
            let childIdentifier = directory.children[1].identifier
            let localDirectory = FileManager.default.temporaryDirectory.appendingPathComponent(directory.identifier)
            defer { try? FileManager.default.removeItem(at: localDirectory) }
            remoteInterface.downloadCompletionHandler = {
                guard remoteInterface.downloadOperationCount == 1 else { return }
                if removeRow {
                    Self.dbManager.removeItemMetadata(ocId: childIdentifier)
                } else {
                    guard var metadata = Self.dbManager.itemMetadata(ocId: childIdentifier) else {
                        XCTFail("Enumeration must have stored the second child")
                        return
                    }
                    metadata.deleted = true
                    metadata.keepDownloaded = true
                    Self.dbManager.addItemMetadata(metadata)
                }
            }

            let (url, fetchedItem, error) = await item.fetchContents(dbManager: Self.dbManager)
            XCTAssertNil(url)
            XCTAssertNil(fetchedItem)
            XCTAssertEqual((error as? NSFileProviderError)?.code, .cannotSynchronize)
            XCTAssertEqual(remoteInterface.downloadOperationCount, 1)
            if removeRow {
                XCTAssertNil(Self.dbManager.itemMetadata(ocId: childIdentifier))
            } else {
                let metadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: childIdentifier))
                XCTAssertTrue(metadata.deleted)
                XCTAssertTrue(metadata.keepDownloaded)
                XCTAssertFalse(metadata.downloaded)
            }
        }
    }

    func testSupersededDirectoryChildStopsFetchingContents() async throws {
        let (directoryItem, directoryRemoteInterface, directory) = makeFetchItem(directory: true)
        let dbManager = Self.dbManager
        let (directoryTask, releaseDirectory) = try await startSuspendedFetch(item: directoryItem, remoteInterface: directoryRemoteInterface)
        let child = directory.children[0]
        let childRemoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        childRemoteInterface.injectMock(Self.account)
        let localDirectory = FileManager.default.temporaryDirectory.appendingPathComponent(directory.identifier)
        defer {
            directoryTask.cancel()
            releaseDirectory.finish()
            try? FileManager.default.removeItem(at: localDirectory)
            removeDownloadedContents(remoteInterface: childRemoteInterface)
        }
        let childItem = try Item(
            metadata: XCTUnwrap(dbManager.itemMetadata(ocId: child.identifier)),
            parentItemIdentifier: NSFileProviderItemIdentifier(directory.identifier),
            account: Self.account,
            remoteInterface: childRemoteInterface,
            dbManager: dbManager
        )
        XCTAssertEqual(directoryRemoteInterface.downloadDestinationURL?.lastPathComponent, child.name)
        let (childURL, fetchedChild, childError) = await childItem.fetchContents(dbManager: dbManager)
        XCTAssertNil(childError)
        XCTAssertTrue(try XCTUnwrap(fetchedChild).isDownloaded)
        var childMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: child.identifier))
        childMetadata.favorite = true
        dbManager.addItemMetadata(childMetadata)

        releaseDirectory.finish()
        let (directoryURL, fetchedDirectory, directoryError) = try await testTaskValue(of: directoryTask)

        XCTAssertNil(directoryURL)
        XCTAssertNil(fetchedDirectory)
        XCTAssertEqual((directoryError as? CocoaError)?.code, .userCancelled)
        XCTAssertEqual(directoryRemoteInterface.downloadOperationCount, 1)
        XCTAssertEqual(try Data(contentsOf: XCTUnwrap(childURL)), child.data)
        let remainingMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: child.identifier))
        XCTAssertTrue(remainingMetadata.downloaded)
        XCTAssertEqual(remainingMetadata.status, Status.normal.rawValue)
        XCTAssertEqual(remainingMetadata.sessionError, "")
        XCTAssertTrue(remainingMetadata.favorite)
        for unrequestedChild in directory.children.dropFirst() {
            XCTAssertEqual(dbManager.itemMetadata(ocId: unrequestedChild.identifier)?.downloaded, false)
            XCTAssertFalse(FileManager.default.fileExists(atPath: localDirectory.appendingPathComponent(unrequestedChild.name).path))
        }
    }

    func testSuccessfulFetchRejectsRemovedOrDeletedMetadata() async throws {
        for removeRow in [true, false] {
            let (item, remoteInterface, remoteItem) = makeFetchItem()
            let dbManager = Self.dbManager
            let (task, release) = try await startSuspendedFetch(item: item, remoteInterface: remoteInterface)
            defer {
                task.cancel()
                release.finish()
                removeDownloadedContents(remoteInterface: remoteInterface)
            }
            if removeRow {
                dbManager.removeItemMetadata(ocId: remoteItem.identifier)
            } else {
                var metadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
                metadata.deleted = true
                dbManager.addItemMetadata(metadata)
            }

            release.finish()
            let (url, fetchedItem, error) = try await testTaskValue(of: task)

            XCTAssertNil(url)
            XCTAssertNil(fetchedItem)
            XCTAssertEqual((error as? NSFileProviderError)?.code, .noSuchItem)
            XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, false)
            if removeRow {
                XCTAssertNil(dbManager.itemMetadata(ocId: remoteItem.identifier))
            } else {
                XCTAssertEqual(dbManager.itemMetadata(ocId: remoteItem.identifier)?.deleted, true)
                XCTAssertEqual(dbManager.itemMetadata(ocId: remoteItem.identifier)?.downloaded, false)
            }
        }
    }

    func testSuccessfulFetchPreservesConcurrentMetadataChanges() async throws {
        let (item, remoteInterface, remoteItem) = makeFetchItem()
        let dbManager = Self.dbManager
        let (task, release) = try await startSuspendedFetch(item: item, remoteInterface: remoteInterface)
        defer {
            task.cancel()
            release.finish()
            removeDownloadedContents(remoteInterface: remoteInterface)
        }
        var metadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        metadata.favorite = true
        metadata.keepDownloaded = true
        dbManager.addItemMetadata(metadata)

        release.finish()
        let (url, fetchedItem, error) = try await testTaskValue(of: task)

        XCTAssertNil(error)
        XCTAssertTrue(try XCTUnwrap(fetchedItem).isDownloaded)
        XCTAssertEqual(try Data(contentsOf: XCTUnwrap(url)), remoteItem.data)
        let remainingMetadata = try XCTUnwrap(dbManager.itemMetadata(ocId: remoteItem.identifier))
        XCTAssertTrue(remainingMetadata.downloaded)
        XCTAssertTrue(remainingMetadata.uploaded)
        XCTAssertEqual(remainingMetadata.status, Status.normal.rawValue)
        XCTAssertEqual(remainingMetadata.sessionError, "")
        XCTAssertTrue(remainingMetadata.favorite)
        XCTAssertTrue(remainingMetadata.keepDownloaded)
    }

    func testCancelledDirectoryEnumerationDoesNotStartChildDownloads() async throws {
        let (item, remoteInterface, _) = makeFetchItem(directory: true)
        let progress = Progress()
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let networkTask = try session.dataTask(with: XCTUnwrap(URL(string: "https://example.invalid/resource")))
        remoteInterface.enumerateCallHandler = { _, _, _, _, _, _, _, taskHandler in
            taskHandler(networkTask)
            progress.cancel()
        }

        let (url, fetchedItem, error) = await item.fetchContents(progress: progress, dbManager: Self.dbManager)

        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        XCTAssertTrue(networkTask.state == .canceling || networkTask.state == .completed)
        XCTAssertEqual(remoteInterface.readOperationCount, 1)
        XCTAssertEqual(remoteInterface.downloadOperationCount, 0)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.status, Status.normal.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.sessionError, "")
    }

    func testCancelledFolderDownloadPreservesCompletedChildrenAndStopsTraversal() async throws {
        let (item, remoteInterface, directory) = makeFetchItem(directory: true)
        let progress = Progress()
        remoteInterface.downloadHandler = {
            if remoteInterface.downloadOperationCount == 2 {
                progress.cancel()
                let cancelled = await waitForCancellation()
                XCTAssertTrue(cancelled, "Cancellation did not reach the operation.")
            }
        }

        let (url, fetchedItem, error) = await item.fetchContents(progress: progress, dbManager: Self.dbManager)
        let localDirectory = FileManager.default.temporaryDirectory.appendingPathComponent(directory.identifier)
        defer { try? FileManager.default.removeItem(at: localDirectory) }
        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        XCTAssertEqual(remoteInterface.downloadOperationCount, 2)
        XCTAssertEqual(remoteInterface.readOperationCount, 1)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: directory.identifier)?.downloaded, false)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: directory.identifier)?.status, Status.normal.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: directory.identifier)?.sessionError, "")
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: directory.children[0].identifier)?.downloaded, true)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: directory.children[1].identifier)?.downloaded, false)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: directory.children[1].identifier)?.status, Status.normal.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: directory.children[1].identifier)?.sessionError, "")
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: directory.children[0].identifier)?.status, Status.normal.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: directory.children[2].identifier)?.downloaded, false)
        XCTAssertEqual(try Data(contentsOf: localDirectory.appendingPathComponent("first.txt")), directory.children[0].data)
        XCTAssertFalse(FileManager.default.fileExists(atPath: localDirectory.appendingPathComponent("second.txt").path))
        XCTAssertFalse(FileManager.default.fileExists(atPath: localDirectory.appendingPathComponent("third.txt").path))
    }

    func testDownloadErrorsPreserveCancellationAndServerErrors() async throws {
        for directory in [false, true] {
            for errorCode in [NSURLErrorCancelled, 404, 503] {
                let (item, remoteInterface, remoteItem) = makeFetchItem(directory: directory)
                defer {
                    if directory {
                        try? FileManager.default.removeItem(at: FileManager.default.temporaryDirectory.appendingPathComponent(remoteItem.identifier))
                    }
                }
                remoteInterface.downloadError = NKError(errorCode: errorCode, errorDescription: "Download failed")
                let (url, fetchedItem, error) = await item.fetchContents(dbManager: Self.dbManager)
                XCTAssertNil(url)
                XCTAssertNil(fetchedItem)
                let cancelled = errorCode == NSURLErrorCancelled
                if cancelled {
                    XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
                } else {
                    XCTAssertEqual((error as? NSFileProviderError)?.code, errorCode == 404 ? .noSuchItem : .serverUnreachable)
                }
                let metadata = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue))
                XCTAssertFalse(metadata.downloaded)
                XCTAssertEqual(metadata.status, cancelled ? Status.normal.rawValue : Status.downloadError.rawValue)
                if cancelled {
                    XCTAssertEqual(metadata.sessionError, "")
                } else {
                    XCTAssertFalse(try XCTUnwrap(metadata.sessionError).isEmpty)
                }
                if directory {
                    let child = try XCTUnwrap(Self.dbManager.itemMetadata(ocId: remoteItem.children[0].identifier))
                    XCTAssertFalse(child.downloaded)
                    XCTAssertEqual(child.status, cancelled ? Status.normal.rawValue : Status.downloadError.rawValue)
                    XCTAssertEqual(child.sessionError, cancelled ? "" : "Download failed")
                }
            }
        }
    }

    func testCancellingCapabilityLookupCancelsUnsharedNetworkTask() async throws {
        await RetrievedCapabilitiesActor.shared.reset()
        let (item, remoteInterface, _) = makeFetchItem()
        let progress = Progress()
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let networkTask = try session.dataTask(with: XCTUnwrap(URL(string: "https://example.invalid/capabilities")))
        defer { removeDownloadedContents(remoteInterface: remoteInterface) }
        remoteInterface.capabilitiesHandler = { _, taskHandler in
            XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, true)
            taskHandler(networkTask)
            progress.cancel()
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation must reach the unshared capabilities fetch")
        }

        let (url, fetchedItem, error) = await item.fetchContents(progress: progress, dbManager: Self.dbManager)

        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        XCTAssertTrue(networkTask.state == .canceling || networkTask.state == .completed)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.downloaded, false)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.status, Status.normal.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.sessionError, "")
        XCTAssertNil(progress.cancellationHandler)
        XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, false)
    }

    func testParentCancellationAfterDownloadRemovesTemporaryContents() async throws {
        await RetrievedCapabilitiesActor.shared.reset()
        let (item, remoteInterface, _) = makeFetchItem()
        let dbManager = Self.dbManager
        defer { removeDownloadedContents(remoteInterface: remoteInterface) }
        let (started, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        remoteInterface.downloadCompletionHandler = {
            XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, true)
            continuation.yield(())
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation did not reach the operation.")
        }
        let task = Task { await item.fetchContents(dbManager: dbManager) }
        defer { task.cancel() }
        guard let _ = try await nextTestValue(from: started) else {
            throw URLError(.badServerResponse)
        }
        task.cancel()

        let (url, fetchedItem, error) = try await testTaskValue(of: task)

        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.downloaded, false)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.status, Status.normal.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.sessionError, "")
        XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, false)
    }

    func testMissingParentLookupSucceedsForContentsAndThumbnails() async throws {
        for thumbnail in [false, true] {
            let (item, remoteInterface, directory) = makeNestedFetchItem()
            remoteInterface.enumerateCallHandler = { path, depth, _, _, _, _, _, _ in
                XCTAssertEqual(path, directory.remotePath)
                XCTAssertEqual(depth, .target)
            }
            if thumbnail {
                let thumbnailData = Data("Thumbnail contents".utf8)
                remoteInterface.thumbnailData = thumbnailData
                let batch = makeThumbnailBatch(identifiers: [item.itemIdentifier], remoteInterface: remoteInterface)
                while let result = try await nextTestValue(from: batch.thumbnails) {
                    XCTAssertEqual(result.identifier, item.itemIdentifier)
                    XCTAssertEqual(result.data, thumbnailData)
                    XCTAssertNil(result.error)
                }
                let completion = try await nextTestValue(from: batch.completion)
                let error: Error? = try XCTUnwrap(completion)
                XCTAssertNil(error)
            } else {
                let (url, fetchedItem, error) = await item.fetchContents(dbManager: Self.dbManager)
                XCTAssertNil(error)
                let localPath = try XCTUnwrap(url)
                defer { try? FileManager.default.removeItem(at: localPath) }
                XCTAssertEqual(try Data(contentsOf: localPath), directory.children[0].data)
                XCTAssertEqual(fetchedItem?.parentItemIdentifier.rawValue, directory.identifier)
                XCTAssertEqual(fetchedItem?.isDownloaded, true)
            }
            XCTAssertEqual(remoteInterface.readOperationCount, 1)
            XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: directory.identifier))
            Self.dbManager.removeItemMetadata(ocId: directory.identifier)
        }
    }

    func testCancellingParentLookupCancelsNetworkTask() async throws {
        let (item, remoteInterface, directory) = makeNestedFetchItem()
        let progress = Progress()
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let networkTask = try session.dataTask(with: XCTUnwrap(URL(string: "https://example.invalid/parent")))
        defer { removeDownloadedContents(remoteInterface: remoteInterface) }
        remoteInterface.enumerateCallHandler = { path, depth, _, _, _, _, _, taskHandler in
            XCTAssertEqual(path, directory.remotePath)
            XCTAssertEqual(depth, .target)
            XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, true)
            taskHandler(networkTask)
            progress.cancel()
        }

        let (url, fetchedItem, error) = await item.fetchContents(progress: progress, dbManager: Self.dbManager)

        XCTAssertNil(url)
        XCTAssertNil(fetchedItem)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
        XCTAssertTrue(networkTask.state == .canceling || networkTask.state == .completed)
        XCTAssertEqual(remoteInterface.readOperationCount, 1)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.downloaded, false)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.status, Status.normal.rawValue)
        XCTAssertEqual(Self.dbManager.itemMetadata(ocId: item.itemIdentifier.rawValue)?.sessionError, "")
        XCTAssertNil(progress.cancellationHandler)
        XCTAssertEqual(remoteInterface.downloadDestinationURL.map { FileManager.default.fileExists(atPath: $0.path) }, false)
    }

    func testCancellingThumbnailPreparationCancelsParentTask() async throws {
        let (item, remoteInterface, directory) = makeNestedFetchItem()
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let networkTask = try session.dataTask(with: XCTUnwrap(URL(string: "https://example.invalid/parent")))
        let (started, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        remoteInterface.enumerateCallHandler = { path, depth, _, _, _, _, _, taskHandler in
            XCTAssertEqual(path, directory.remotePath)
            XCTAssertEqual(depth, .target)
            taskHandler(networkTask)
            continuation.yield(())
        }
        remoteInterface.enumerateHandler = {
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation did not reach the operation.")
        }
        remoteInterface.thumbnailHandler = { _, _ in XCTFail("Cancelled preparation must not start a thumbnail transfer") }
        let batch = makeThumbnailBatch(identifiers: [item.itemIdentifier], remoteInterface: remoteInterface)

        guard let _ = try await nextTestValue(from: started) else {
            throw URLError(.badServerResponse)
        }
        batch.progress.cancel()

        while let result = try await nextTestValue(from: batch.thumbnails) {
            XCTAssertEqual(result.identifier, item.itemIdentifier)
            XCTAssertNil(result.data)
            XCTAssertEqual((result.error as? CocoaError)?.code, .userCancelled)
        }
        XCTAssertTrue(networkTask.state == .canceling || networkTask.state == .completed)
        XCTAssertEqual(remoteInterface.readOperationCount, 1)
        XCTAssertEqual(remoteInterface.downloadOperationCount, 0)
    }

    func testCancellingThumbnailPreparationCancelsUnsharedCapabilityTask() async throws {
        await RetrievedCapabilitiesActor.shared.reset()
        let (item, remoteInterface, _) = makeFetchItem(preview: true)
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let networkTask = try session.dataTask(with: XCTUnwrap(URL(string: "https://example.invalid/capabilities")))
        let (started, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        remoteInterface.capabilitiesHandler = { _, taskHandler in
            taskHandler(networkTask)
            continuation.yield(())
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation must reach the unshared capabilities fetch")
        }
        remoteInterface.thumbnailHandler = { _, _ in XCTFail("Cancelled preparation must not start a thumbnail transfer") }
        let batch = makeThumbnailBatch(identifiers: [item.itemIdentifier], remoteInterface: remoteInterface)
        guard let _ = try await nextTestValue(from: started) else {
            throw URLError(.badServerResponse)
        }
        batch.progress.cancel()

        while let result = try await nextTestValue(from: batch.thumbnails) {
            XCTAssertEqual(result.identifier, item.itemIdentifier)
            XCTAssertNil(result.data)
            XCTAssertEqual((result.error as? CocoaError)?.code, .userCancelled)
        }
        let completion = try await nextTestValue(from: batch.completion)
        XCTAssertEqual((completion.flatMap(\.self) as? CocoaError)?.code, .userCancelled)
        XCTAssertTrue(networkTask.state == .canceling || networkTask.state == .completed)
        XCTAssertEqual(remoteInterface.readOperationCount, 0)
        XCTAssertNil(batch.progress.cancellationHandler)
        await RetrievedCapabilitiesActor.shared.awaitFetchCompletion(forAccount: Self.account.ncKitAccount)
        let cached = await RetrievedCapabilitiesActor.shared.getCapabilities(for: Self.account.ncKitAccount)
        XCTAssertNil(cached)
    }

    func testCancellingThumbnailCancelsNetworkTask() async throws {
        let (item, remoteInterface, _) = makeFetchItem(preview: true)
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let networkTask = try session.dataTask(with: XCTUnwrap(URL(string: "https://example.invalid/preview")))
        let (started, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        remoteInterface.thumbnailHandler = { _, taskHandler in
            taskHandler(networkTask)
            continuation.yield(())
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation did not reach the operation.")
        }
        let batch = makeThumbnailBatch(identifiers: [item.itemIdentifier], remoteInterface: remoteInterface)
        defer { batch.progress.cancel() }
        let startedValue = try await nextTestValue(from: started)
        _ = try XCTUnwrap(startedValue)
        batch.progress.cancel()

        let thumbnailValue = try await nextTestValue(from: batch.thumbnails)
        let thumbnail = try XCTUnwrap(thumbnailValue)
        XCTAssertEqual(thumbnail.identifier, item.itemIdentifier)
        XCTAssertNil(thumbnail.data)
        XCTAssertEqual((thumbnail.error as? CocoaError)?.code, .userCancelled)
        let nextThumbnail = try await nextTestValue(from: batch.thumbnails)
        XCTAssertNil(nextThumbnail)
        let completion = try await nextTestValue(from: batch.completion)
        XCTAssertEqual((completion.flatMap(\.self) as? CocoaError)?.code, .userCancelled)
        XCTAssertTrue(networkTask.state == .canceling || networkTask.state == .completed)
        XCTAssertNil(batch.progress.cancellationHandler)
    }

    func testCancelledProgressDoesNotStartThumbnailDownload() async {
        let (item, remoteInterface, _) = makeFetchItem(preview: true)
        let progress = Progress()
        progress.cancel()
        remoteInterface.thumbnailHandler = { _, _ in XCTFail("Cancelled thumbnail must not start a transfer") }

        let (data, error) = await item.performFetchThumbnail(
            size: CGSize(width: 32, height: 32), domain: nil, progress: progress,
            cancellation: NetworkOperationCancellation(log: FileProviderLogMock())
        )

        XCTAssertNil(data)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
    }

    func testCancelledThumbnailDoesNotReturnSuccessfulResponseData() async {
        let (item, remoteInterface, _) = makeFetchItem(preview: true)
        let progress = Progress()
        remoteInterface.thumbnailData = Data("Late thumbnail response".utf8)
        remoteInterface.thumbnailCompletionHandler = { progress.cancel() }

        let (data, error) = await item.performFetchThumbnail(
            size: CGSize(width: 32, height: 32), domain: nil, progress: progress,
            cancellation: NetworkOperationCancellation(log: FileProviderLogMock())
        )

        XCTAssertNil(data)
        XCTAssertEqual((error as? CocoaError)?.code, .userCancelled)
    }

    func testCancelledThumbnailBatchCancelsEveryTransfer() async throws {
        let (first, remoteInterface, _) = makeFetchItem(preview: true)
        let (second, _, _) = makeFetchItem(preview: true)
        let identifiers = [first.itemIdentifier, second.itemIdentifier]
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let (started, continuation) = AsyncStream<URLSessionTask>.makeStream()
        defer { continuation.finish() }
        remoteInterface.thumbnailHandler = { url, taskHandler in
            let networkTask = session.dataTask(with: url)
            taskHandler(networkTask)
            continuation.yield(networkTask)
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation did not reach the operation.")
        }
        let batch = makeThumbnailBatch(identifiers: identifiers, remoteInterface: remoteInterface)
        var networkTasks: [URLSessionTask] = []
        for _ in identifiers {
            let startedTask = try await nextTestValue(from: started)
            try networkTasks.append(XCTUnwrap(startedTask))
        }
        batch.progress.cancel()

        var reportedIdentifiers: [NSFileProviderItemIdentifier] = []
        while let result = try await nextTestValue(from: batch.thumbnails) {
            reportedIdentifiers.append(result.identifier)
            XCTAssertNil(result.data)
            XCTAssertEqual((result.error as? CocoaError)?.code, .userCancelled)
        }
        let completion = try await nextTestValue(from: batch.completion)
        XCTAssertEqual((completion.flatMap(\.self) as? CocoaError)?.code, .userCancelled)
        XCTAssertEqual(reportedIdentifiers.count, identifiers.count)
        XCTAssertEqual(Set(reportedIdentifiers), Set(identifiers))
        XCTAssertTrue(networkTasks.allSatisfy { $0.state == .canceling || $0.state == .completed })
        XCTAssertNil(batch.progress.cancellationHandler)
    }

    func testThumbnailBatchReportsSuccessfulContents() async throws {
        let (first, remoteInterface, _) = makeFetchItem(preview: true)
        let (second, _, _) = makeFetchItem(preview: true)
        let identifiers = [first.itemIdentifier, second.itemIdentifier]
        let thumbnailData = Data("Thumbnail contents".utf8)
        remoteInterface.thumbnailData = thumbnailData
        let batch = makeThumbnailBatch(identifiers: identifiers, remoteInterface: remoteInterface)
        var reportedIdentifiers: [NSFileProviderItemIdentifier] = []
        while let result = try await nextTestValue(from: batch.thumbnails) {
            reportedIdentifiers.append(result.identifier)
            XCTAssertEqual(result.data, thumbnailData)
            XCTAssertNil(result.error)
        }
        let completion = try await nextTestValue(from: batch.completion)
        let error: Error? = try XCTUnwrap(completion)
        XCTAssertNil(error)
        XCTAssertEqual(reportedIdentifiers.count, identifiers.count)
        XCTAssertEqual(Set(reportedIdentifiers), Set(identifiers))
        XCTAssertEqual(batch.progress.completedUnitCount, Int64(identifiers.count))
    }

    func testThumbnailBatchReportsPerItemFailures() async throws {
        let (item, remoteInterface, _) = makeFetchItem(preview: true)
        remoteInterface.thumbnailError = NKError(errorCode: 503, errorDescription: "Server unavailable")
        let missingIdentifier = NSFileProviderItemIdentifier("missing-item")
        let batch = makeThumbnailBatch(identifiers: [item.itemIdentifier, missingIdentifier], remoteInterface: remoteInterface)
        var reportedIdentifiers: [NSFileProviderItemIdentifier] = []
        while let result = try await nextTestValue(from: batch.thumbnails) {
            reportedIdentifiers.append(result.identifier)
            XCTAssertNil(result.data)
            XCTAssertEqual((result.error as? NSFileProviderError)?.code, result.identifier == missingIdentifier ? .noSuchItem : .serverUnreachable)
        }
        let completion = try await nextTestValue(from: batch.completion)
        let error: Error? = try XCTUnwrap(completion)
        XCTAssertNil(error)
        XCTAssertEqual(Set(reportedIdentifiers), [item.itemIdentifier, missingIdentifier])
    }

    func testEmptyThumbnailBatchFinishesImmediately() async {
        let (_, remoteInterface, _) = makeFetchItem()
        let completed = expectation(description: "Empty thumbnail batch finishes")
        let progress = NextcloudFileProviderKit.fetchThumbnails(
            for: [],
            requestedSize: CGSize(width: 32, height: 32),
            account: Self.account,
            usingRemoteInterface: remoteInterface,
            andDatabase: Self.dbManager,
            perThumbnailCompletionHandler: { _, _, _ in XCTFail("No thumbnail was requested") },
            log: FileProviderLogMock(),
            completionHandler: { error in
                XCTAssertNil(error)
                completed.fulfill()
            }
        )
        await fulfillment(of: [completed], timeout: 5)
        XCTAssertEqual(progress.totalUnitCount, 0)
    }

    func testFetchFileContents() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.injectMock(Self.account)

        let remoteItem = MockRemoteItem(
            identifier: "item",
            versionIdentifier: "0",
            name: "item.txt",
            remotePath: Self.account.davFilesUrl + "/item.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteItem]
        remoteItem.parent = rootItem

        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(itemMetadata)
        XCTAssertNotNil(itemMetadata.ocId)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (localPathMaybe, fetchedItemMaybe, error) = await item.fetchContents(dbManager: Self.dbManager)
        XCTAssertNil(error)
        let localPath = try XCTUnwrap(localPathMaybe)
        let fetchedItem = try XCTUnwrap(fetchedItemMaybe)
        let contents = try Data(contentsOf: localPath)

        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: itemMetadata.ocId))

        XCTAssertEqual(contents, remoteItem.data)
        XCTAssertTrue(fetchedItem.isDownloaded)
        XCTAssertTrue(fetchedItem.isUploaded)
        XCTAssertEqual(fetchedItem.itemIdentifier, item.itemIdentifier)
        XCTAssertEqual(fetchedItem.filename, item.filename)
        XCTAssertEqual(fetchedItem.creationDate, item.creationDate)
    }

    func testFetchAliasFileContentsDetectsAliasType() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.injectMock(Self.account)

        // Construct minimal data starting with the Apple Bookmark magic "book" (0x62 0x6F 0x6F 0x6B)
        var aliasData = Data([0x62, 0x6F, 0x6F, 0x6B])
        aliasData.append(contentsOf: [UInt8](repeating: 0, count: 60))

        let remoteItem = MockRemoteItem(
            identifier: "aliasItem",
            versionIdentifier: "0",
            name: "My Alias", // no extension, no MIME type — as the server would present it
            remotePath: Self.account.davFilesUrl + "/My Alias",
            data: aliasData,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteItem]
        remoteItem.parent = rootItem

        // contentType is empty — as it arrives from the server with no MIME type
        let itemMetadata = remoteItem.toItemMetadata(account: Self.account)
        XCTAssertTrue(itemMetadata.contentType.isEmpty)
        Self.dbManager.addItemMetadata(itemMetadata)

        let item = Item(
            metadata: itemMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (_, fetchedItemMaybe, error) = await item.fetchContents(dbManager: Self.dbManager)
        XCTAssertNil(error)
        let fetchedItem = try XCTUnwrap(fetchedItemMaybe)

        XCTAssertEqual(fetchedItem.contentType, UTType.aliasFile)

        let storedMetadata = Self.dbManager.itemMetadata(ocId: itemMetadata.ocId)
        XCTAssertEqual(storedMetadata?.contentType, UTType.aliasFile.identifier)
    }

    func testFetchDirectoryContents() async throws {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: rootItem)
        remoteInterface.injectMock(Self.account)

        let remoteDirectory = MockRemoteItem(
            identifier: "directory",
            versionIdentifier: "0",
            name: "directory",
            remotePath: Self.account.davFilesUrl + "/directory",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteDirectoryChildFile = MockRemoteItem(
            identifier: "childFile",
            versionIdentifier: "0",
            name: "file.txt",
            remotePath: remoteDirectory.remotePath + "/file.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteDirectoryChildDirA = MockRemoteItem(
            identifier: "childDirectoryA",
            versionIdentifier: "0",
            name: "directoryA",
            remotePath: remoteDirectory.remotePath + "/directoryA",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteDirectoryChildDirB = MockRemoteItem(
            identifier: "childDirectoryB",
            versionIdentifier: "0",
            name: "directoryB",
            remotePath: remoteDirectory.remotePath + "/directoryB",
            directory: true,
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        let remoteDirectoryChildDirBChildFile = MockRemoteItem(
            identifier: "childDirectoryBChildFile",
            versionIdentifier: "0",
            name: "dirBfile.txt",
            remotePath: remoteDirectoryChildDirB.remotePath + "/dirBfile.txt",
            data: "Hello, World!".data(using: .utf8),
            account: Self.account.ncKitAccount,
            username: Self.account.username,
            userId: Self.account.id,
            serverUrl: Self.account.serverUrl
        )
        rootItem.children = [remoteDirectory]
        remoteDirectory.parent = rootItem
        remoteDirectory.children = [
            remoteDirectoryChildFile, remoteDirectoryChildDirA, remoteDirectoryChildDirB
        ]
        remoteDirectoryChildFile.parent = remoteDirectory
        remoteDirectoryChildDirA.parent = remoteDirectory
        remoteDirectoryChildDirB.parent = remoteDirectory
        remoteDirectoryChildDirB.children = [remoteDirectoryChildDirBChildFile]
        remoteDirectoryChildDirBChildFile.parent = remoteDirectoryChildDirB

        let directoryMetadata = remoteDirectory.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryMetadata)

        let directoryChildFileMetadata =
            remoteDirectoryChildFile.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryChildFileMetadata)

        let directoryChildDirAMetadata =
            remoteDirectoryChildDirA.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryChildDirAMetadata)

        let directoryChildDirBMetadata =
            remoteDirectoryChildDirB.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryChildDirBMetadata)

        let directoryChildDirBChildFileMetadata =
            remoteDirectoryChildDirBChildFile.toItemMetadata(account: Self.account)
        Self.dbManager.addItemMetadata(directoryChildDirBChildFileMetadata)

        let item = Item(
            metadata: directoryMetadata,
            parentItemIdentifier: .rootContainer,
            account: Self.account,
            remoteInterface: remoteInterface,
            dbManager: Self.dbManager
        )

        let (localPathMaybe, fetchedItemMaybe, error) =
            await item.fetchContents(dbManager: Self.dbManager)
        XCTAssertNil(error)
        let localPath = try XCTUnwrap(localPathMaybe)
        let fetchedItem = try XCTUnwrap(fetchedItemMaybe)

        XCTAssertNotNil(Self.dbManager.itemMetadata(ocId: directoryMetadata.ocId))

        XCTAssertEqual(fetchedItem.itemIdentifier, item.itemIdentifier)
        XCTAssertEqual(fetchedItem.filename, item.filename)
        XCTAssertEqual(fetchedItem.creationDate, item.creationDate)
        XCTAssertTrue(fetchedItem.isUploaded)
        XCTAssertTrue(fetchedItem.isDownloaded)

        let fm = FileManager.default
        var itemIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(atPath: localPath.path, isDirectory: &itemIsDir))
        XCTAssertTrue(itemIsDir.boolValue)

        let itemChildFileUrl = localPath.appendingPathComponent("file.txt")
        let itemChildFilePath = itemChildFileUrl.path
        var itemChildFileIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(atPath: itemChildFilePath, isDirectory: &itemChildFileIsDir))
        XCTAssertFalse(itemChildFileIsDir.boolValue)
        XCTAssertEqual(try Data(contentsOf: itemChildFileUrl), remoteDirectoryChildFile.data)

        let itemChildDirAPath = localPath.appendingPathComponent("directoryA").path
        var itemChildDirAIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(atPath: itemChildDirAPath, isDirectory: &itemChildDirAIsDir))
        XCTAssertTrue(itemChildDirAIsDir.boolValue)

        let itemChildDirBUrl = localPath.appendingPathComponent("directoryB")
        let itemChildDirBPath = itemChildDirBUrl.path
        var itemChildDirBIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(atPath: itemChildDirBPath, isDirectory: &itemChildDirBIsDir))
        XCTAssertTrue(itemChildDirBIsDir.boolValue)

        let itemChildDirBChildFileUrl = itemChildDirBUrl.appendingPathComponent("dirBfile.txt")
        let itemChildDirBChildFilePath = itemChildDirBChildFileUrl.path
        var itemChildDirBChildFileIsDir = ObjCBool(false)
        XCTAssertTrue(fm.fileExists(
            atPath: itemChildDirBChildFilePath, isDirectory: &itemChildDirBChildFileIsDir
        ))
        XCTAssertFalse(itemChildDirBChildFileIsDir.boolValue)
        XCTAssertEqual(
            try Data(contentsOf: itemChildDirBChildFileUrl), remoteDirectoryChildDirBChildFile.data
        )
    }
}
