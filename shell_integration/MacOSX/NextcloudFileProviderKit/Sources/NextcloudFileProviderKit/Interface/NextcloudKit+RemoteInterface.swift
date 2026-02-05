//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
@preconcurrency import FileProvider
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit

extension NextcloudKit: RemoteInterface {
    public func setDelegate(_ delegate: any NextcloudKitDelegate) {
        setup(delegate: delegate)
    }

    public func createFolder(
        remotePath: String,
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, ocId: String?, date: NSDate?, error: NKError) {
        await withCheckedContinuation { continuation in
            createFolder(
                serverUrlFileName: remotePath,
                account: account.ncKitAccount,
                options: options,
                taskHandler: taskHandler
            ) { account, ocId, date, _, error in
                continuation.resume(returning: (account, ocId, date as NSDate?, error))
            }
        }
    }

    public func upload(
        remotePath: String,
        localPath: String,
        creationDate: Date? = nil,
        modificationDate: Date? = nil,
        account: Account,
        options: NKRequestOptions = .init(),
        requestHandler: @escaping (UploadRequest) -> Void = { _ in },
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in },
        progressHandler: @escaping (Progress) -> Void = { _ in }
    ) async -> (
        account: String,
        ocId: String?,
        etag: String?,
        date: NSDate?,
        size: Int64,
        response: HTTPURLResponse?,
        remoteError: NKError
    ) {
        await withCheckedContinuation { continuation in
            upload(
                serverUrlFileName: remotePath,
                fileNameLocalPath: localPath,
                dateCreationFile: creationDate,
                dateModificationFile: modificationDate,
                account: account.ncKitAccount,
                options: options,
                requestHandler: requestHandler,
                taskHandler: taskHandler,
                progressHandler: progressHandler
            ) { account, ocId, etag, date, size, response, nkError in
                continuation.resume(returning: (
                    account,
                    ocId,
                    etag,
                    date as NSDate?,
                    size,
                    response?.response,
                    nkError
                ))
            }
        }
    }

    public func chunkedUpload(
        localPath: String,
        remotePath: String,
        remoteChunkStoreFolderName: String = UUID().uuidString,
        chunkSize: Int,
        remainingChunks: [RemoteFileChunk],
        creationDate: Date? = nil,
        modificationDate: Date? = nil,
        account: Account,
        options: NKRequestOptions = .init(),
        currentNumChunksUpdateHandler _: @escaping (_ num: Int) -> Void = { _ in },
        chunkCounter _: @escaping (_ counter: Int) -> Void = { _ in },
        log: any FileProviderLogging,
        chunkUploadStartHandler: @escaping (_ filesChunk: [RemoteFileChunk]) -> Void = { _ in },
        requestHandler _: @escaping (_ request: UploadRequest) -> Void = { _ in },
        taskHandler: @escaping (_ task: URLSessionTask) -> Void = { _ in },
        progressHandler: @escaping (Progress) -> Void = { _ in },
        chunkUploadCompleteHandler: @escaping (_ fileChunk: RemoteFileChunk) -> Void = { _ in }
    ) async -> (account: String, file: NKFile?, nkError: NKError) {
        let logger = FileProviderLogger(category: "NextcloudKit+RemoteInterface", log: log)

        guard let remoteUrl = URL(string: remotePath) else {
            return ("", nil, .urlError)
        }
        let localUrl = URL(fileURLWithPath: localPath)

        let fm = FileManager.default
        let chunksOutputDirectoryUrl =
            fm.temporaryDirectory.appendingPathComponent(remoteChunkStoreFolderName)
        do {
            try fm.createDirectory(at: chunksOutputDirectoryUrl, withIntermediateDirectories: true)
        } catch {
            logger.error(
                """
                Could not create temporary directory for chunked files: \(error)
                """
            )
            return ("", nil, .urlError)
        }

        var directory = localUrl.deletingLastPathComponent().path
        if directory.last == "/" {
            directory.removeLast()
        }
        let fileChunksOutputDirectory = chunksOutputDirectoryUrl.path
        let fileName = localUrl.lastPathComponent
        let destinationFileName = remoteUrl.lastPathComponent
        guard let serverUrl = remoteUrl
            .deletingLastPathComponent()
            .absoluteString
            .removingPercentEncoding
        else {
            logger.error(
                "NCKit ext: Could not get server url from \(remotePath)"
            )
            return ("", nil, .urlError)
        }
        let fileChunks = remainingChunks.toNcKitChunks()

        logger.info(
            """
            Beginning chunked upload of: \(localPath)
                directory: \(directory)
                fileChunksOutputDirectory: \(fileChunksOutputDirectory)
                fileName: \(fileName)
                destinationFileName: \(destinationFileName)
                date: \(modificationDate?.debugDescription ?? "")
                creationDate: \(creationDate?.debugDescription ?? "")
                serverUrl: \(serverUrl)
                chunkFolder: \(remoteChunkStoreFolderName)
                filesChunk: \(fileChunks)
                chunkSize: \(chunkSize)
            """
        )

        do {
            let (account, file) = try await uploadChunkAsync(
                directory: directory,
                fileChunksOutputDirectory: fileChunksOutputDirectory,
                fileName: fileName,
                destinationFileName: destinationFileName,
                date: modificationDate,
                creationDate: creationDate,
                serverUrl: serverUrl,
                chunkFolder: remoteChunkStoreFolderName,
                filesChunk: fileChunks,
                chunkSize: chunkSize,
                account: account.ncKitAccount,
                options: options,
                uploadStart: { processedChunks in
                    let chunks = RemoteFileChunk.fromNcKitChunks(processedChunks, remoteChunkStoreFolderName: remoteChunkStoreFolderName)
                    chunkUploadStartHandler(chunks)
                },
                uploadTaskHandler: taskHandler,
                uploadProgressHandler: { totalBytesExpected, totalBytes, _ in
                    let currentProgress = Progress(totalUnitCount: totalBytesExpected)
                    currentProgress.completedUnitCount = totalBytes
                    progressHandler(currentProgress)
                },
                uploaded: { uploadedChunk in
                    let chunk = RemoteFileChunk(ncKitChunk: uploadedChunk, remoteChunkStoreFolderName: remoteChunkStoreFolderName)
                    chunkUploadCompleteHandler(chunk)
                }
            )

            return (account, file, .success)
        } catch let nkError as NKError {
            return (account.ncKitAccount, nil, nkError)
        } catch {
            return (account.ncKitAccount, nil, NKError.urlError)
        }
    }

    public func move(
        remotePathSource: String,
        remotePathDestination: String,
        overwrite: Bool,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        await withCheckedContinuation { continuation in
            moveFileOrFolder(
                serverUrlFileNameSource: remotePathSource,
                serverUrlFileNameDestination: remotePathDestination,
                overwrite: overwrite,
                account: account.ncKitAccount,
                options: options,
                taskHandler: taskHandler
            ) { account, data, error in
                continuation.resume(returning: (account, data?.data, error))
            }
        }
    }

    public func enumerate(
        remotePath: String,
        depth: EnumerateDepth,
        showHiddenFiles: Bool = false,
        includeHiddenFiles: [String] = [],
        requestBody: Data? = nil,
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (
        account: String, files: [NKFile], data: AFDataResponse<Data>?, error: NKError
    ) {
        await withCheckedContinuation { continuation in
            readFileOrFolder(
                serverUrlFileName: remotePath,
                depth: depth.rawValue,
                showHiddenFiles: showHiddenFiles,
                includeHiddenFiles: includeHiddenFiles,
                requestBody: requestBody,
                account: account.ncKitAccount,
                options: options,
                taskHandler: taskHandler
            ) { account, files, data, error in
                continuation.resume(returning: (account, files ?? [], data, error))
            }
        }
    }

    public func delete(
        remotePath: String,
        account: Account,
        options _: NKRequestOptions = .init(),
        taskHandler _: @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, response: HTTPURLResponse?, error: NKError) {
        await withCheckedContinuation { continuation in
            deleteFileOrFolder(
                serverUrlFileName: remotePath, account: account.ncKitAccount
            ) { account, response, error in
                continuation.resume(returning: (account, response?.response, error))
            }
        }
    }

    public func lockUnlockFile(serverUrlFileName: String, type: NKLockType?, shouldLock: Bool, account: Account, options: NKRequestOptions, taskHandler: @escaping (URLSessionTask) -> Void) async throws -> NKLock? {
        try await lockUnlockFile(serverUrlFileName: serverUrlFileName, type: type, shouldLock: shouldLock, account: account.ncKitAccount, options: options, taskHandler: taskHandler)
    }

    public func restoreFromTrash(
        filename: String,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (_ task: URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        let trashFileUrl = account.trashUrl + "/" + filename
        let recoverFileUrl = account.trashRestoreUrl + "/" + filename

        return await move(
            remotePathSource: trashFileUrl,
            remotePathDestination: recoverFileUrl,
            overwrite: true,
            account: account,
            options: options,
            taskHandler: taskHandler
        )
    }

    public func downloadThumbnail(
        url: URL,
        account: Account,
        options: NKRequestOptions,
        taskHandler: @escaping (URLSessionTask) -> Void
    ) async -> (account: String, data: Data?, error: NKError) {
        await withCheckedContinuation { continuation in
            downloadPreview(
                url: url, account: account.ncKitAccount, options: options, taskHandler: taskHandler
            ) { account, data, error in
                continuation.resume(returning: (account, data?.data, error))
            }
        }
    }

    public func fetchCapabilities(
        account: Account,
        options: NKRequestOptions = .init(),
        taskHandler: @escaping (_ task: URLSessionTask) -> Void = { _ in }
    ) async -> (account: String, capabilities: Capabilities?, data: Data?, error: NKError) {
        let ncKitAccount = account.ncKitAccount
        await RetrievedCapabilitiesActor.shared.setOngoingFetch(
            forAccount: ncKitAccount, ongoing: true
        )
        let result = await withCheckedContinuation { continuation in
            getCapabilities(account: account.ncKitAccount, options: options, taskHandler: taskHandler) { account, capabilities, responseData, error in
                let capabilities: Capabilities? = {
                    guard let realData = responseData?.data else { return nil }
                    return Capabilities(data: realData)
                }()
                continuation.resume(returning: (account, capabilities, responseData?.data, error))
            }
        }
        await RetrievedCapabilitiesActor.shared.setOngoingFetch(
            forAccount: ncKitAccount, ongoing: false
        )
        if let capabilities = result.1 {
            await RetrievedCapabilitiesActor.shared.setCapabilities(
                forAccount: account.ncKitAccount, capabilities: capabilities
            )
        }
        return result
    }

    public func tryAuthenticationAttempt(
        account: Account,
        options _: NKRequestOptions = .init(),
        taskHandler _: @escaping (_ task: URLSessionTask) -> Void = { _ in }
    ) async -> AuthenticationAttemptResultState {
        // Test by trying to fetch user profile
        let (_, _, _, error) =
            await enumerate(remotePath: account.davFilesUrl + "/", depth: .target, account: account)

        if error != .success {
            nkLog(error: "Error in auth check: \(error.errorDescription)")
        }

        if error == .success {
            return .success
        } else if error.isCouldntConnectError {
            return .connectionError
        } else {
            return .authenticationError
        }
    }
}
