//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
import Foundation
import NextcloudCapabilitiesKit
import NextcloudFileProviderKit
import NextcloudKit

///
/// Passthrough ``RemoteInterface`` that records the request traffic flowing through it.
///
/// This is not a stand-in for the server: every call is forwarded to the wrapped interface, which in
/// the benchmarks is the real `NextcloudKit` talking to a real Nextcloud over HTTP. The decorator
/// exists so a scenario can report *how many* requests the code under measurement issued and how
/// many of them were in flight at once, alongside the wall clock. A wall clock on its own cannot
/// distinguish "the code got smarter" from "the server was warmer".
///
final class CountingRemoteInterface: RemoteInterface, @unchecked Sendable {
    private let wrapped: RemoteInterface
    private let lock = NSLock()

    private var _enumerateCount = 0
    private var _downloadCount = 0
    private var _uploadCount = 0
    private var _inFlightEnumerations = 0
    private var _maxConcurrentEnumerations = 0
    private var _enumeratedPaths: [String] = []

    init(wrapping wrapped: RemoteInterface) {
        self.wrapped = wrapped
    }

    // MARK: - Recorded traffic

    struct Traffic {
        var enumerations: Int
        var downloads: Int
        var uploads: Int
        var maxConcurrentEnumerations: Int
        var enumeratedPaths: [String]
    }

    var traffic: Traffic {
        lock.lock()
        defer { lock.unlock() }
        return Traffic(
            enumerations: _enumerateCount,
            downloads: _downloadCount,
            uploads: _uploadCount,
            maxConcurrentEnumerations: _maxConcurrentEnumerations,
            enumeratedPaths: _enumeratedPaths
        )
    }

    func reset() {
        lock.lock()
        defer { lock.unlock() }
        _enumerateCount = 0
        _downloadCount = 0
        _uploadCount = 0
        _inFlightEnumerations = 0
        _maxConcurrentEnumerations = 0
        _enumeratedPaths = []
    }

    private func beginEnumeration(_ remotePath: String) {
        lock.lock()
        defer { lock.unlock() }
        _enumerateCount += 1
        _enumeratedPaths.append(remotePath)
        _inFlightEnumerations += 1
        _maxConcurrentEnumerations = max(_maxConcurrentEnumerations, _inFlightEnumerations)
    }

    private func endEnumeration() {
        lock.lock()
        defer { lock.unlock() }
        _inFlightEnumerations -= 1
    }

    private func countDownload() {
        lock.lock()
        defer { lock.unlock() }
        _downloadCount += 1
    }

    private func countUpload() {
        lock.lock()
        defer { lock.unlock() }
        _uploadCount += 1
    }

    // MARK: - Instrumented

    func enumerate(
        remotePath: String,
        depth: EnumerateDepth,
        showHiddenFiles: Bool,
        includeHiddenFiles: [String],
        requestBody: Data?,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> (account: String, files: [NKFile], data: AFDataResponse<Data>?, error: NKError) {
        beginEnumeration(remotePath)
        defer { endEnumeration() }

        return await wrapped.enumerate(
            remotePath: remotePath,
            depth: depth,
            showHiddenFiles: showHiddenFiles,
            includeHiddenFiles: includeHiddenFiles,
            requestBody: requestBody,
            account: account,
            options: options,
            taskHandler: taskHandler
        )
    }

    func downloadAsync(
        serverUrlFileName: Any,
        fileNameLocalPath: String,
        account: String,
        options: NKRequestOptions,
        requestHandler: @escaping (DownloadRequest) -> Void,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void,
        progressHandler: @escaping (Progress) -> Void
    ) async -> (account: String, response: AFDownloadResponse<URL?>?, nkError: NKError) {
        countDownload()

        return await wrapped.downloadAsync(
            serverUrlFileName: serverUrlFileName,
            fileNameLocalPath: fileNameLocalPath,
            account: account,
            options: options,
            requestHandler: requestHandler,
            taskHandler: taskHandler,
            progressHandler: progressHandler
        )
    }

    func upload(
        remotePath: String,
        localPath: String,
        creationDate: Date?,
        modificationDate: Date?,
        account: Account,
        options: NKRequestOptions,
        requestHandler: @escaping (UploadRequest) -> Void,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void,
        progressHandler: @escaping (Progress) -> Void
    ) async -> (
        account: String, ocId: String?, etag: String?, date: NSDate?, size: Int64,
        response: HTTPURLResponse?, remoteError: NKError
    ) {
        countUpload()

        return await wrapped.upload(
            remotePath: remotePath,
            localPath: localPath,
            creationDate: creationDate,
            modificationDate: modificationDate,
            account: account,
            options: options,
            requestHandler: requestHandler,
            taskHandler: taskHandler,
            progressHandler: progressHandler
        )
    }

    // MARK: - Plain forwarding

    func setDelegate(_ delegate: NextcloudKitDelegate) {
        wrapped.setDelegate(delegate)
    }

    func createFolder(
        remotePath: String,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> (account: String, ocId: String?, date: NSDate?, error: NKError) {
        await wrapped.createFolder(
            remotePath: remotePath, account: account, options: options, taskHandler: taskHandler
        )
    }

    func chunkedUpload(
        localPath: String,
        remotePath: String,
        remoteChunkStoreFolderName: String,
        chunkSize: Int,
        remainingChunks: [RemoteFileChunk],
        creationDate: Date?,
        modificationDate: Date?,
        account: Account,
        options: NKRequestOptions,
        currentNumChunksUpdateHandler: @escaping (Int) -> Void,
        chunkCounter: @escaping (Int) -> Void,
        log: any FileProviderLogging,
        chunkUploadStartHandler: @escaping ([RemoteFileChunk]) -> Void,
        requestHandler: @escaping (UploadRequest) -> Void,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void,
        progressHandler: @escaping (Progress) -> Void,
        chunkUploadCompleteHandler: @escaping (RemoteFileChunk) -> Void
    ) async -> (account: String, file: NKFile?, chunksDirectory: URL?, nkError: NKError) {
        await wrapped.chunkedUpload(
            localPath: localPath,
            remotePath: remotePath,
            remoteChunkStoreFolderName: remoteChunkStoreFolderName,
            chunkSize: chunkSize,
            remainingChunks: remainingChunks,
            creationDate: creationDate,
            modificationDate: modificationDate,
            account: account,
            options: options,
            currentNumChunksUpdateHandler: currentNumChunksUpdateHandler,
            chunkCounter: chunkCounter,
            log: log,
            chunkUploadStartHandler: chunkUploadStartHandler,
            requestHandler: requestHandler,
            taskHandler: taskHandler,
            progressHandler: progressHandler,
            chunkUploadCompleteHandler: chunkUploadCompleteHandler
        )
    }

    func removeLocalChunks(remoteChunkStoreFolderName: String) throws {
        try wrapped.removeLocalChunks(remoteChunkStoreFolderName: remoteChunkStoreFolderName)
    }

    func move(
        remotePathSource: String,
        remotePathDestination: String,
        overwrite: Bool,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        await wrapped.move(
            remotePathSource: remotePathSource,
            remotePathDestination: remotePathDestination,
            overwrite: overwrite,
            account: account,
            options: options,
            taskHandler: taskHandler
        )
    }

    func delete(
        remotePath: String,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> (account: String, response: HTTPURLResponse?, error: NKError) {
        await wrapped.delete(
            remotePath: remotePath, account: account, options: options, taskHandler: taskHandler
        )
    }

    func lockUnlockFile(
        serverUrlFileName: String,
        type: NKLockType?,
        shouldLock: Bool,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async throws -> NKLock? {
        try await wrapped.lockUnlockFile(
            serverUrlFileName: serverUrlFileName,
            type: type,
            shouldLock: shouldLock,
            account: account,
            options: options,
            taskHandler: taskHandler
        )
    }

    func listingTrashAsync(
        filename: String?,
        showHiddenFiles: Bool,
        account: String,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> (
        account: String, items: [NKTrash]?, responseData: AFDataResponse<Data>?, error: NKError
    ) {
        await wrapped.listingTrashAsync(
            filename: filename,
            showHiddenFiles: showHiddenFiles,
            account: account,
            options: options,
            taskHandler: taskHandler
        )
    }

    func restoreFromTrash(
        filename: String,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        await wrapped.restoreFromTrash(
            filename: filename, account: account, options: options, taskHandler: taskHandler
        )
    }

    func downloadThumbnail(
        url: URL,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        await wrapped.downloadThumbnail(
            url: url, account: account, options: options, taskHandler: taskHandler
        )
    }

    func fetchCapabilities(
        account: Account,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> (account: String, capabilities: Capabilities?, data: Data?, error: NKError) {
        await wrapped.fetchCapabilities(
            account: account, options: options, taskHandler: taskHandler
        )
    }

    func getUserProfileAsync(
        account: String,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> (
        account: String, userProfile: NKUserProfile?, responseData: AFDataResponse<Data>?,
        error: NKError
    ) {
        await wrapped.getUserProfileAsync(
            account: account, options: options, taskHandler: taskHandler
        )
    }

    func tryAuthenticationAttempt(
        account: Account,
        options: NKRequestOptions,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void
    ) async -> AuthenticationAttemptResultState {
        await wrapped.tryAuthenticationAttempt(
            account: account, options: options, taskHandler: taskHandler
        )
    }
}
