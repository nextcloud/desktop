//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
@preconcurrency import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
@testable import NextcloudKit
import RealmSwift
import TestInterface
import XCTest

final class UploadTests: NextcloudFileProviderKitTestCase {
    static let account = Account(user: "user", id: "id", serverUrl: "test.cloud.com", password: "1234")
    static let dbManager = try! FilesDatabaseManager(account: account, databaseDirectory: makeDatabaseDirectory(), fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"), log: FileProviderLogMock())

    override func setUp() {
        super.setUp()
        setUpDatabase(Self.dbManager)
    }

    private func makeChunkUploadHTTP(observeStoppingChunk: String? = nil) throws -> (
        account: Account,
        kit: NextcloudKit,
        requests: AsyncStream<MockHTTPURLProtocol>,
        stoppedRequests: AsyncStream<URLRequest>,
        cleanup: @Sendable () -> Void
    ) {
        let host = "\(UUID().uuidString.lowercased()).example.invalid"
        let account = Account(user: "user", id: "id", serverUrl: "https://\(host)", password: "password")
        let parentPath = try XCTUnwrap(URL(string: account.davFilesUrl)).path
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MockHTTPURLProtocol.self]
        let session = Alamofire.Session(configuration: configuration)
        let kit = NextcloudKit()
        kit.nkCommonInstance.nksessions.append(NKSession(
            nkCommonInstance: kit.nkCommonInstance,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id,
            password: account.password,
            account: account.ncKitAccount,
            userAgent: "UploadTests",
            groupIdentifier: "",
            httpMaximumConnectionsPerHost: 1,
            httpMaximumConnectionsPerHostInDownload: 1,
            httpMaximumConnectionsPerHostInUpload: 1,
            sessionDataOverride: session
        ))

        let (requests, requestEvents) = AsyncStream<MockHTTPURLProtocol>.makeStream()
        let (stoppedRequests, stopEvents) = AsyncStream<URLRequest>.makeStream()
        MockHTTPURLProtocol.setHandlers(
            forHost: host,
            start: { http in
                if http.request.httpMethod == "PROPFIND",
                   let path = http.request.url?.path,
                   path == parentPath || path.hasPrefix("/remote.php/dav/uploads/")
                {
                    let listing = """
                    <d:multistatus xmlns:d="DAV:">
                      <d:response><d:href>\(path)/</d:href><d:propstat>
                        <d:prop><d:resourcetype><d:collection/></d:resourcetype><d:quota-available-bytes>1024</d:quota-available-bytes></d:prop>
                        <d:status>HTTP/1.1 200 OK</d:status>
                      </d:propstat></d:response>
                    </d:multistatus>
                    """
                    http.respond(status: 207, headers: ["Content-Type": "application/xml"], body: Data(listing.utf8))
                } else {
                    requestEvents.yield(http)
                }
            },
            stop: { request in
                if request.httpMethod == "PUT", request.url?.lastPathComponent == observeStoppingChunk {
                    stopEvents.yield(request)
                }
            }
        )
        let cleanup: @Sendable () -> Void = {
            session.session.invalidateAndCancel()
            MockHTTPURLProtocol.removeHandlers(forHost: host)
            requestEvents.finish()
            stopEvents.finish()
        }
        return (account, kit, requests, stoppedRequests, cleanup)
    }

    private func startChunkUpload(
        fileURL: URL,
        destination: URL,
        kit: NextcloudKit,
        account: Account,
        itemIdentifier: String,
        modificationDate: Date,
        progress: Progress = .init(),
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void = { _ in }
    ) -> Task<(ocId: String?, etag: String?, date: Date?, size: Int64?, remoteError: NKError), Never> {
        let dbManager = Self.dbManager
        return Task.detached { @Sendable in
            await NextcloudFileProviderKit.upload(
                fileLocatedAt: fileURL.path,
                toRemotePath: destination.absoluteString,
                usingRemoteInterface: kit,
                withAccount: account,
                inChunksSized: 3,
                forItemWithIdentifier: itemIdentifier,
                dbManager: dbManager,
                progress: progress,
                modificationDate: modificationDate,
                log: FileProviderLogMock(),
                taskHandler: taskHandler
            )
        }
    }

    private func nextUploadRequest(
        from requests: AsyncStream<MockHTTPURLProtocol>,
        method: String,
        name: String,
        file: StaticString = #filePath,
        line: UInt = #line
    ) async throws -> MockHTTPURLProtocol {
        let event = try await nextTestValue(from: requests)
        let http = try XCTUnwrap(event, file: file, line: line)
        XCTAssertEqual(http.request.httpMethod, method, file: file, line: line)
        XCTAssertEqual(http.request.url?.lastPathComponent, name, file: file, line: line)
        return http
    }

    private func assertDestination(
        _ actual: URL?,
        matches expected: URL,
        file: StaticString = #filePath,
        line: UInt = #line
    ) {
        XCTAssertEqual(actual?.scheme, expected.scheme, file: file, line: line)
        XCTAssertEqual(actual?.host, expected.host, file: file, line: line)
        // The SDK can emit redundant separators when assembling a file at the DAV root.
        XCTAssertEqual(actual?.pathComponents, expected.pathComponents, file: file, line: line)
    }

    private func checkCancellationDuringChunkAssembly(
        moveStatus: Int,
        headers: [String: String] = [:],
        readbackAttempts: Int = 0,
        expectedError: NKError
    ) async throws {
        let (account, kit, requests, _, cleanup) = try makeChunkUploadHTTP()
        defer { cleanup() }
        let data = Data(repeating: 1, count: 8)
        let fileURL = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try data.write(to: fileURL)
        defer { try? FileManager.default.removeItem(at: fileURL) }
        let itemIdentifier = UUID().uuidString
        let modificationDate = Date(timeIntervalSince1970: 1_700_000_000)
        let uploadId = chunkUploadIdentifier(forItemWithIdentifier: itemIdentifier, fileSize: Int64(data.count), modificationDate: modificationDate)
        defer { try? kit.removeLocalChunks(remoteChunkStoreFolderName: uploadId) }
        let destination = try XCTUnwrap(URL(string: account.davFilesUrl + "/file.txt"))
        let upload = startChunkUpload(
            fileURL: fileURL,
            destination: destination,
            kit: kit,
            account: account,
            itemIdentifier: itemIdentifier,
            modificationDate: modificationDate
        )
        defer { upload.cancel() }

        for name in ["1", "2", "3"] {
            let chunk = try await nextUploadRequest(from: requests, method: "PUT", name: name)
            chunk.respond(status: 201)
        }
        let move = try await nextUploadRequest(from: requests, method: "MOVE", name: ".file")
        assertDestination(move.request.value(forHTTPHeaderField: "Destination").flatMap { URL(string: $0) }, matches: destination)
        if readbackAttempts == 0 {
            upload.cancel()
        }
        move.respond(status: moveStatus, headers: headers)

        for attempt in 0 ..< readbackAttempts {
            let readback = try await nextUploadRequest(from: requests, method: "PROPFIND", name: "file.txt")
            assertDestination(readback.request.url, matches: destination)
            if attempt == 0 {
                upload.cancel()
            }
            readback.respond(status: 404)
        }

        let result = try await testTaskValue(of: upload)
        XCTAssertEqual(result.remoteError.errorCode, expectedError.errorCode)
        XCTAssertEqual(result.ocId, headers["OC-FileID"])
        XCTAssertEqual(result.etag, headers["OC-ETag"])
        XCTAssertEqual(result.size, expectedError == .success ? Int64(data.count) : nil)
        XCTAssertTrue(try XCTUnwrap(Self.dbManager.remoteFileChunks(uploadId: uploadId)).isEmpty)
    }

    func testCancellationDuringUploadCancelsNetworkHandlesAndAllowsRetry() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }
        let root = MockRemoteItem.rootItem(account: Self.account)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: root)
        let progress = Progress()
        let session = Session(startRequestsImmediately: false)
        let request = session.upload(Data(), to: "https://example.invalid/resource")
        let urlSession = URLSession(configuration: .ephemeral)
        defer { urlSession.invalidateAndCancel() }
        let (networkTask, networkCompletion) = try observedNetworkTask(
            in: urlSession,
            url: XCTUnwrap(URL(string: "https://example.invalid/resource"))
        )
        remoteInterface.uploadRequest = request
        remoteInterface.uploadTask = networkTask
        remoteInterface.uploadHandler = {
            progress.cancel()
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation must reach the upload operation")
        }
        let itemIdentifier = UUID().uuidString

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: data.count,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            progress: progress,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError.errorCode, NSURLErrorCancelled)
        XCTAssertNil(result.ocId)
        let cancellationCode = try await nextTestValue(from: networkCompletion)
        XCTAssertEqual(cancellationCode, NSURLErrorCancelled)
        XCTAssertTrue(request.isCancelled)
        XCTAssertNil(progress.cancellationHandler)
        XCTAssertNil(progress.pausingHandler)
        XCTAssertNil(progress.resumingHandler)
        XCTAssertEqual(remoteInterface.uploadOperationCount, 1)
        XCTAssertTrue(root.children.isEmpty)

        remoteInterface.uploadHandler = nil
        remoteInterface.uploadRequest = nil
        remoteInterface.uploadTask = nil
        let retry = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: data.count,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(retry.remoteError, .success)
        XCTAssertEqual(remoteInterface.uploadOperationCount, 2)
        XCTAssertEqual(root.children.count, 1)
        XCTAssertEqual(root.children.first?.data, data)
    }

    func testCancellingChunkUploadStopsInFlightPUTAndResumesRemainingChunks() async throws {
        let (account, kit, requests, stoppedRequests, cleanup) = try makeChunkUploadHTTP(observeStoppingChunk: "2")
        defer { cleanup() }
        let fileURL = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileURL)
        defer { try? FileManager.default.removeItem(at: fileURL) }
        let itemIdentifier = UUID().uuidString
        let modificationDate = Date(timeIntervalSince1970: 1_700_000_000)
        let uploadId = chunkUploadIdentifier(forItemWithIdentifier: itemIdentifier, fileSize: Int64(data.count), modificationDate: modificationDate)
        let chunksDirectory = FileManager.default.temporaryDirectory.appendingPathComponent(uploadId, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: chunksDirectory) }
        let progress = Progress()
        let (putTasks, taskEvents) = AsyncStream<URLSessionTask>.makeStream()
        defer { taskEvents.finish() }
        let destination = try XCTUnwrap(URL(string: account.davFilesUrl + "/file.txt"))
        let upload = startChunkUpload(
            fileURL: fileURL,
            destination: destination,
            kit: kit,
            account: account,
            itemIdentifier: itemIdentifier,
            modificationDate: modificationDate,
            progress: progress,
            taskHandler: { task in
                if task.originalRequest?.httpMethod == "PUT", task.originalRequest?.url?.lastPathComponent == "2" {
                    taskEvents.yield(task)
                }
            }
        )
        defer { upload.cancel() }

        let first = try await nextUploadRequest(from: requests, method: "PUT", name: "1")
        first.respond(status: 201)
        let interrupted = try await nextUploadRequest(from: requests, method: "PUT", name: "2")
        let reportedTask = try await nextTestValue(from: putTasks)
        XCTAssertEqual(try XCTUnwrap(reportedTask).originalRequest?.url, interrupted.request.url)
        progress.cancel()

        let stopped = try await nextTestValue(from: stoppedRequests)
        XCTAssertEqual(try XCTUnwrap(stopped).url, interrupted.request.url)
        let result = try await testTaskValue(of: upload)
        XCTAssertEqual(result.remoteError.errorCode, NSURLErrorCancelled)
        XCTAssertNil(result.ocId)
        XCTAssertNil(progress.cancellationHandler)
        let remainingChunks = try XCTUnwrap(Self.dbManager.remoteFileChunks(uploadId: uploadId))
        XCTAssertEqual(remainingChunks.map(\.fileName).sorted(), ["2", "3"])
        for chunk in remainingChunks {
            XCTAssertTrue(FileManager.default.fileExists(atPath: chunksDirectory.appendingPathComponent(chunk.fileName).path))
        }

        let retry = startChunkUpload(
            fileURL: fileURL,
            destination: destination,
            kit: kit,
            account: account,
            itemIdentifier: itemIdentifier,
            modificationDate: modificationDate
        )
        defer { retry.cancel() }

        for name in ["2", "3"] {
            let chunk = try await nextUploadRequest(from: requests, method: "PUT", name: name)
            chunk.respond(status: 201)
        }
        let move = try await nextUploadRequest(from: requests, method: "MOVE", name: ".file")
        assertDestination(move.request.value(forHTTPHeaderField: "Destination").flatMap { URL(string: $0) }, matches: destination)
        move.respond(status: 201, headers: ["OC-FileID": "assembled-id", "OC-ETag": "assembled-etag"])

        let retried = try await testTaskValue(of: retry)
        XCTAssertEqual(retried.remoteError, .success)
        XCTAssertEqual(retried.ocId, "assembled-id")
        XCTAssertEqual(retried.etag, "assembled-etag")
        XCTAssertEqual(retried.size, Int64(data.count))
        XCTAssertTrue(try XCTUnwrap(Self.dbManager.remoteFileChunks(uploadId: uploadId)).isEmpty)
        XCTAssertFalse(FileManager.default.fileExists(atPath: chunksDirectory.path))
    }

    func testCancellationDuringPreflightDoesNotStartUpload() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data(repeating: 1, count: 8).write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }
        let root = MockRemoteItem.rootItem(account: Self.account)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: root)
        let progress = Progress()
        remoteInterface.enumerateHandler = {
            progress.cancel()
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Cancellation must reach the quota lookup")
        }

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: 8,
            forItemWithIdentifier: UUID().uuidString,
            dbManager: Self.dbManager,
            progress: progress,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError.errorCode, NSURLErrorCancelled)
        XCTAssertEqual(remoteInterface.uploadOperationCount, 0)
        XCTAssertTrue(root.children.isEmpty)
    }

    func testCancellationErrorMappingForCollisions() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let error = NKError(errorCode: NSURLErrorCancelled, errorDescription: "Cancelled")
        let mappedError = await error.fileProviderError(
            handlingCollisionAgainstItemInRemotePath: Self.account.davFilesUrl + "/file.txt",
            dbManager: Self.dbManager,
            remoteInterface: remoteInterface,
            log: FileProviderLogMock()
        )
        XCTAssertEqual((mappedError as? CocoaError)?.code, .userCancelled)
    }

    func testChunkPreparationErrorMappingForCollisionsWithoutCancellation() async {
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        for description in ["Files empty.", "Chunking was cancelled."] {
            let error = NKError(errorCode: -5, errorDescription: description)
            let mappedError = await error.fileProviderError(
                handlingCollisionAgainstItemInRemotePath: Self.account.davFilesUrl + "/file.txt",
                dbManager: Self.dbManager,
                remoteInterface: remoteInterface,
                log: FileProviderLogMock()
            )
            XCTAssertEqual((mappedError as? NSFileProviderError)?.code, .cannotSynchronize)
            XCTAssertNotEqual((mappedError as? CocoaError)?.code, .userCancelled)
        }
        XCTAssertEqual(remoteInterface.readOperationCount, 0)
    }

    func testCancellationDuringChunkPreparationReturnsCancellation() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data(repeating: 1, count: 8).write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }
        let root = MockRemoteItem.rootItem(account: Self.account)
        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: root)
        let progress = Progress()
        remoteInterface.chunkPreparationHandler = {
            progress.cancel()
            let cancelled = await waitForCancellation()
            XCTAssertTrue(cancelled, "Chunk preparation must receive cancellation")
        }

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: 3,
            forItemWithIdentifier: UUID().uuidString,
            dbManager: Self.dbManager,
            progress: progress,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError.errorCode, NSURLErrorCancelled)
        XCTAssertEqual((result.remoteError.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: .init("file")) as? CocoaError)?.code, .userCancelled)
        XCTAssertNil(result.ocId)
        XCTAssertTrue(progress.isCancelled)
        XCTAssertEqual(remoteInterface.uploadOperationCount, 0)
        XCTAssertTrue(root.children.isEmpty)
        XCTAssertTrue(remoteInterface.chunkUploadDirectories.isEmpty)
    }

    func testChunkPreparationErrorsArePreserved() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data(repeating: 1, count: 8).write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }
        let emptyChunksError = NKError(error: NSError(
            domain: "chunkedFile",
            code: -5,
            userInfo: [NSLocalizedDescriptionKey: "Files empty."]
        ))
        let scenarios = [
            (NKError(errorCode: -4, errorDescription: "Could not write chunks."), false),
            (emptyChunksError, false),
            (NKError(errorCode: -5, errorDescription: "Chunking was cancelled."), false),
            (emptyChunksError, true)
        ]
        for (preparationError, cancelProgress) in scenarios {
            let root = MockRemoteItem.rootItem(account: Self.account)
            let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: root)
            remoteInterface.chunkPreparationError = preparationError
            let progress = Progress()
            if cancelProgress {
                remoteInterface.chunkPreparationHandler = { progress.cancel() }
            }

            let result = await NextcloudFileProviderKit.upload(
                fileLocatedAt: fileUrl.path,
                toRemotePath: Self.account.davFilesUrl + "/file.txt",
                usingRemoteInterface: remoteInterface,
                withAccount: Self.account,
                inChunksSized: 3,
                forItemWithIdentifier: UUID().uuidString,
                dbManager: Self.dbManager,
                progress: progress,
                log: FileProviderLogMock()
            )

            XCTAssertEqual(result.remoteError.errorCode, preparationError.errorCode)
            XCTAssertEqual(result.remoteError.errorDescription, preparationError.errorDescription)
            let mappedError = result.remoteError.fileProviderError(handlingNoSuchItemErrorUsingItemIdentifier: .init("file"))
            XCTAssertEqual((mappedError as? NSFileProviderError)?.code, .cannotSynchronize)
            XCTAssertNotEqual((mappedError as? CocoaError)?.code, .userCancelled)
            XCTAssertEqual(progress.isCancelled, cancelProgress)
            XCTAssertNil(result.ocId)
            XCTAssertEqual(remoteInterface.uploadOperationCount, 0)
            XCTAssertTrue(root.children.isEmpty)
        }
    }

    func testCancellationDuringRejectedChunkAssemblyPreservesServerError() async throws {
        try await checkCancellationDuringChunkAssembly(
            moveStatus: 507,
            expectedError: NKError(errorCode: 507, errorDescription: "Insufficient quota.")
        )
    }

    func testCancellationDuringSuccessfulChunkAssemblyPreservesFileIdentity() async throws {
        try await checkCancellationDuringChunkAssembly(
            moveStatus: 201,
            headers: ["OC-FileID": "assembled-id", "OC-ETag": "assembled-etag"],
            expectedError: .success
        )
    }

    func testCancellationAfterSuccessfulMovePreservesFailedReadbackError() async throws {
        try await checkCancellationDuringChunkAssembly(
            moveStatus: 201,
            readbackAttempts: 3,
            expectedError: .errorChunkMoveFile
        )
    }

    func testStandardUpload() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            forItemWithIdentifier: "standard-upload-item",
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)
    }

    func testChunkedUpload() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let chunkSize = 3
        let itemIdentifier = "chunked-upload-\(UUID().uuidString)"
        let chunkDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("mock-chunks-\(UUID().uuidString)", isDirectory: true)
        remoteInterface.chunkUploadDirectory = chunkDirectory
        defer { try? FileManager.default.removeItem(at: chunkDirectory) }

        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { chunk in
                uploadedChunks.record(chunk, fileExists: FileManager.default.fileExists(
                    atPath: chunkDirectory.appendingPathComponent(chunk.fileName).path
                ))
            }
        )
        let expectedChunkCount = Int(ceil(Double(data.count) / Double(chunkSize)))

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.last)
        let lastUploadedChunkNameInt = try XCTUnwrap(Int(lastUploadedChunk.fileName))
        XCTAssertEqual(firstUploadedChunkNameInt, 1)
        XCTAssertEqual(lastUploadedChunkNameInt, expectedChunkCount)
        XCTAssertEqual(Int(firstUploadedChunk.size), chunkSize)
        XCTAssertEqual(
            Int(lastUploadedChunk.size), data.count - ((lastUploadedChunkNameInt - 1) * chunkSize)
        )
        XCTAssertTrue(uploadedChunks.chunksExisted)
        XCTAssertFalse(FileManager.default.fileExists(atPath: chunkDirectory.path))
        XCTAssertEqual(
            Self.dbManager.remoteChunkStoreFolderNames()?
                .filter { $0.hasPrefix(chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)) }
                .count,
            0
        )
    }

    func testFailedChunkedUploadKeepsTemporaryChunksForResume() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        remoteInterface.uploadError = NKError(statusCode: 500, fallbackDescription: "Upload failed")
        remoteInterface.chunkUploadCompletedChunkCount = 1

        let chunkSize = 3
        let itemIdentifier = "failed-chunked-upload-\(UUID().uuidString)"
        let chunkDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("mock-chunks-\(UUID().uuidString)", isDirectory: true)
        remoteInterface.chunkUploadDirectory = chunkDirectory
        defer { try? FileManager.default.removeItem(at: chunkDirectory) }

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError.errorCode, 500)
        XCTAssertTrue(FileManager.default.fileExists(atPath: chunkDirectory.path))
        let storedChunks = try FileManager.default.contentsOfDirectory(atPath: chunkDirectory.path)
        XCTAssertEqual(storedChunks.count, Int(ceil(Double(data.count) / Double(chunkSize))))
        XCTAssertEqual(
            Self.dbManager.remoteChunkStoreFolderNames()?
                .filter { $0.hasPrefix(chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)) }
                .count,
            2
        )
    }

    func testChunkedUploadIsNotStartedWhenTheRecordedChunksCannotBeRead() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try Data(repeating: 1, count: 8).write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }

        let remoteInterface = MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let chunkDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("mock-chunks-\(UUID().uuidString)", isDirectory: true)
        remoteInterface.chunkUploadDirectory = chunkDirectory
        defer { try? FileManager.default.removeItem(at: chunkDirectory) }

        // Starting from scratch would overwrite the chunks recorded for a resumable upload.
        try Self.dbManager.breakTableForTesting(RemoteFileChunkRecord.databaseTableName)
        defer { Self.dbManager.recreateTablesForTesting() }

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: 3,
            forItemWithIdentifier: "unreadable-chunks-\(UUID().uuidString)",
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError, .metadataDatabaseUnreadable)
        XCTAssertNil(result.ocId)
        XCTAssertFalse(FileManager.default.fileExists(atPath: chunkDirectory.path), "No chunk was uploaded.")
    }

    func testFailedChunkedUploadWithoutRemainingChunksRemovesTemporaryDirectory() async throws {
        let fileUrl = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }

        let remoteInterface = MockRemoteInterface(
            account: Self.account,
            rootItem: MockRemoteItem.rootItem(account: Self.account)
        )
        remoteInterface.uploadError = NKError(statusCode: 500, fallbackDescription: "Upload failed")

        let itemIdentifier = "failed-complete-chunks-\(UUID().uuidString)"
        let chunkDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("mock-chunks-\(UUID().uuidString)", isDirectory: true)
        remoteInterface.chunkUploadDirectory = chunkDirectory
        defer { try? FileManager.default.removeItem(at: chunkDirectory) }

        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: Self.account.davFilesUrl + "/file.txt",
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: 3,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            log: FileProviderLogMock()
        )

        XCTAssertEqual(result.remoteError.errorCode, 500)
        XCTAssertFalse(FileManager.default.fileExists(atPath: chunkDirectory.path))
        XCTAssertEqual(
            Self.dbManager.remoteChunkStoreFolderNames()?
                .filter { $0.hasPrefix(chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)) }
                .count,
            0
        )
    }

    func testResumingInterruptedChunkedUpload() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let chunkSize = 3

        // The chunk id is derived from (item, size, modificationDate). Seed the prior attempt's
        // bookkeeping under exactly that derived id so the resume path recognises identical content.
        let itemIdentifier = "resume-item"
        let modificationDate = Date(timeIntervalSince1970: 1_700_000_000)
        let uploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: Int64(data.count),
            modificationDate: modificationDate
        )

        let previousUploadedChunkNum = 1
        let previousUploadedChunk = RemoteFileChunk(
            fileName: String(previousUploadedChunkNum),
            size: Int64(chunkSize),
            remoteChunkStoreFolderName: uploadId
        )
        remoteInterface.currentChunks = [uploadId: [previousUploadedChunk]]

        Self.dbManager.addRemoteFileChunks([
            RemoteFileChunk(
                fileName: String(previousUploadedChunkNum + 1),
                size: Int64(chunkSize),
                remoteChunkStoreFolderName: uploadId
            ),
            RemoteFileChunk(
                fileName: String(previousUploadedChunkNum + 2),
                size: Int64(data.count - (chunkSize * (previousUploadedChunkNum + 1))),
                remoteChunkStoreFolderName: uploadId
            )
        ])

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            modificationDate: modificationDate,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.record($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        // Only the not-yet-uploaded chunks (2 and 3) are re-sent; chunk 1 is resumed from the server.
        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.first)
        let firstUploadedChunkNameInt = try XCTUnwrap(Int(firstUploadedChunk.fileName))
        let lastUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.last)
        let lastUploadedChunkNameInt = try XCTUnwrap(Int(lastUploadedChunk.fileName))
        XCTAssertEqual(firstUploadedChunkNameInt, previousUploadedChunkNum + 1)
        XCTAssertEqual(lastUploadedChunkNameInt, previousUploadedChunkNum + 2)
        XCTAssertEqual(Int(firstUploadedChunk.size), chunkSize)
        XCTAssertEqual(
            Int(lastUploadedChunk.size), data.count - ((lastUploadedChunkNameInt - 1) * chunkSize)
        )
    }

    func testUsingServerCapabilitiesChunkSize() async throws {
        let capabilities = ##"""
        {
            "ocs": {
                "meta": {
                    "status": "ok",
                    "statuscode": 100,
                    "message": "OK",
                    "totalitems": "",
                    "itemsperpage": ""
                },
                "data": {
                    "version": {
                        "major": 28,
                        "minor": 0,
                        "micro": 4,
                        "string": "28.0.4",
                        "edition": "",
                        "extendedSupport": false
                    },
                    "capabilities": {
                        "core": {
                            "pollinterval": 60,
                            "webdav-root": "remote.php/webdav",
                            "reference-api": true,
                            "reference-regex": "(\\s|\n|^)(https?:\\/\\/)((?:[-A-Z0-9+_]+\\.)+[-A-Z]+(?:\\/[-A-Z0-9+&@#%?=~_|!:,.;()]*)*)(\\s|\n|$)"
                        },
                        "files": {
                            "bigfilechunking": true,
                            "blacklisted_files": [
                                ".htaccess"
                            ],
                            "chunked_upload": {
                                "max_size": 4,
                                "max_parallel_count": 5
                            },
                            "directEditing": {
                                "url": "https://mock.nc.com/ocs/v2.php/apps/files/api/v1/directEditing",
                                "etag": "c748e8fc588b54fc5af38c4481a19d20",
                                "supportsFileId": true
                            },
                            "comments": true,
                            "undelete": true,
                            "versioning": true,
                            "version_labeling": true,
                            "version_deletion": true
                        },
                        "dav": {
                            "chunking": "1.0",
                            "bulkupload": "1.0"
                        }
                    }
                }
            }
        }
        """##
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        remoteInterface.capabilities = capabilities

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            forItemWithIdentifier: "caps-chunk-size-item",
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.record($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        XCTAssertEqual(uploadedChunks.chunks.first?.size, 4)
        XCTAssertEqual(uploadedChunks.chunks.last?.size, 4)
    }

    func testUsingServerCapabilitiesWithoutChunkSize() async throws {
        let capabilities = ##"""
        {
            "ocs": {
                "meta": {
                    "status": "ok",
                    "statuscode": 100,
                    "message": "OK",
                    "totalitems": "",
                    "itemsperpage": ""
                },
                "data": {
                    "version": {
                        "major": 28,
                        "minor": 0,
                        "micro": 4,
                        "string": "28.0.4",
                        "edition": "",
                        "extendedSupport": false
                    },
                    "capabilities": {
                        "core": {
                            "pollinterval": 60,
                            "webdav-root": "remote.php/webdav",
                            "reference-api": true,
                            "reference-regex": "(\\s|\n|^)(https?:\\/\\/)((?:[-A-Z0-9+_]+\\.)+[-A-Z]+(?:\\/[-A-Z0-9+&@#%?=~_|!:,.;()]*)*)(\\s|\n|$)"
                        },
                        "files": {
                            "bigfilechunking": true,
                            "blacklisted_files": [
                                ".htaccess"
                            ],
                            "comments": true,
                            "undelete": true,
                            "versioning": true,
                            "version_labeling": true,
                            "version_deletion": true
                        },
                        "dav": {
                            "chunking": "1.0",
                            "bulkupload": "1.0"
                        }
                    }
                }
            }
        }
        """##
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: defaultFileChunkSize + 1)
        try data.write(to: fileUrl)

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        remoteInterface.capabilities = capabilities

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            forItemWithIdentifier: "caps-no-chunk-size-item",
            dbManager: Self.dbManager,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.record($0) }
        )

        XCTAssertEqual(result.remoteError, .success)
        XCTAssertEqual(result.size, Int64(data.count))
        XCTAssertNotNil(result.ocId)
        XCTAssertNotNil(result.etag)

        XCTAssertEqual(uploadedChunks.chunks.first?.size, Int64(defaultFileChunkSize))
        XCTAssertEqual(uploadedChunks.chunks.last?.size, 1)
    }

    /// F3 content-safety: a prior interrupted chunked upload of a *different* version of the same item
    /// (different derived id) must NOT be resumed. Its stale chunk bookkeeping is swept and the upload
    /// starts fresh, so old chunks can never be spliced into the new content.
    func testChunkedUploadDiscardsStaleChunksAfterContentChange() async throws {
        let fileUrl =
            FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let data = Data(repeating: 1, count: 8)
        try data.write(to: fileUrl)
        defer { try? FileManager.default.removeItem(at: fileUrl) }

        let remoteInterface =
            MockRemoteInterface(account: Self.account, rootItem: MockRemoteItem.rootItem(account: Self.account))
        let chunkSize = 3
        let itemIdentifier = "content-change-item"

        // Seed an older attempt whose individual chunk rows have already been consumed.
        let staleModificationDate = Date(timeIntervalSince1970: 1_600_000_000)
        let staleUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: Int64(data.count),
            modificationDate: staleModificationDate
        )
        let staleChunksDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("stale-chunks-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(
            at: staleChunksDirectory,
            withIntermediateDirectories: true
        )
        try Data([1]).write(to: staleChunksDirectory.appendingPathComponent("2"))
        remoteInterface.chunkUploadDirectories[staleUploadId] = staleChunksDirectory
        defer { try? FileManager.default.removeItem(at: staleChunksDirectory) }

        var metadata = SendableItemMetadata(
            ocId: itemIdentifier,
            fileName: "file.txt",
            account: Self.account
        )
        metadata.status = Status.uploadError.rawValue
        metadata.chunkUploadId = staleUploadId
        Self.dbManager.addItemMetadata(metadata)

        // The newer version (different mtime) derives a different id.
        let newModificationDate = Date(timeIntervalSince1970: 1_700_000_000)
        let newUploadId = chunkUploadIdentifier(
            forItemWithIdentifier: itemIdentifier,
            fileSize: Int64(data.count),
            modificationDate: newModificationDate
        )
        XCTAssertNotEqual(staleUploadId, newUploadId)

        let remotePath = Self.account.davFilesUrl + "/file.txt"
        let uploadedChunks = UploadChunkRecorder()
        let result = await NextcloudFileProviderKit.upload(
            fileLocatedAt: fileUrl.path,
            toRemotePath: remotePath,
            usingRemoteInterface: remoteInterface,
            withAccount: Self.account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: itemIdentifier,
            dbManager: Self.dbManager,
            modificationDate: newModificationDate,
            log: FileProviderLogMock(),
            chunkUploadCompleteHandler: { uploadedChunks.record($0) }
        )

        XCTAssertEqual(result.remoteError, .success)

        // The old metadata pointer is consumed before it is replaced with the new attempt.
        XCTAssertNil(Self.dbManager.itemMetadata(ocId: itemIdentifier)?.chunkUploadId)
        XCTAssertFalse(FileManager.default.fileExists(atPath: staleChunksDirectory.path))

        // The upload started fresh (chunk 1 was re-sent, not resumed from chunk 2).
        let firstUploadedChunk = try XCTUnwrap(uploadedChunks.chunks.first)
        XCTAssertEqual(Int(firstUploadedChunk.fileName), 1)
    }
}
