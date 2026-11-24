//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
import CryptoKit
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit

let defaultFileChunkSize = 104_857_600 // 100 MiB

/// The per-item prefix shared by every chunked-upload identifier for a given item, so stale chunk
/// bookkeeping from earlier versions of the same item can be swept without matching another item.
func chunkUploadIdentifierPrefix(forItemWithIdentifier itemIdentifier: String) -> String {
    let encodedIdentifier = Data(itemIdentifier.utf8)
        .map { String(format: "%02x", $0) }
        .joined()
    return encodedIdentifier + "_"
}

/// Derives the identifier for a chunked upload's server folder and local `RemoteFileChunk` records.
///
/// The item, size and modification date identify the version whose chunks can be resumed.
/// Creates also bind the identifier to the destination because the server remembers the original target.
/// The encoded item prefix lets cleanup find older uploads for the same item.
///
/// When no modification date is available the content can't be bound to an id, so a per-attempt unique
/// id is used: this forgoes resume for that upload but never risks a bad splice. In the File Provider
/// model a real content change always bumps `contentModificationDate` (that is how the change was
/// detected), so equal `(size, modificationDate)` reliably means identical content.
func chunkUploadIdentifier(
    forItemWithIdentifier itemIdentifier: String,
    fileSize: Int64,
    modificationDate: Date?,
    createDestination: String? = nil
) -> String {
    let prefix = chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)

    guard let modificationDate else {
        return prefix + UUID().uuidString
    }

    let mtimeSeconds = Int64(modificationDate.timeIntervalSince1970.rounded())
    let contentIdentifier = "\(prefix)\(fileSize)_\(mtimeSeconds)"
    guard let createDestination else { return contentIdentifier }

    let destinationHash = SHA256.hash(data: Data(createDestination.utf8))
        .map { String(format: "%02x", $0) }
        .joined()
    return "\(contentIdentifier)_\(destinationHash)"
}

typealias UploadResult = (
    ocId: String?,
    etag: String?,
    date: Date?,
    size: Int64?,
    failure: UploadFailure?,
    createAttempt: PendingCreateUploadRecord?
)

func upload(
    fileLocatedAt localFilePath: String,
    toRemotePath remotePath: String,
    sourceMetadata: SendableItemMetadata? = nil,
    usingRemoteInterface remoteInterface: RemoteInterface,
    withAccount account: Account,
    inChunksSized chunkSize: Int? = nil,
    forItemWithIdentifier itemIdentifier: String,
    dbManager: FilesDatabaseManager,
    progress: Progress = Progress(),
    creationDate: Date? = nil,
    modificationDate: Date? = nil,
    options: NKRequestOptions = .init(queue: .global(qos: .utility)),
    creatingNewFile: Bool = false,
    log: any FileProviderLogging,
    requestHandler: @Sendable @escaping (UploadRequest) -> Void = { _ in },
    taskHandler: @Sendable @escaping (URLSessionTask) -> Void = { _ in },
    progressHandler: @Sendable @escaping (Progress) -> Void = { _ in },
    chunkUploadCompleteHandler: @Sendable @escaping (_ fileChunk: RemoteFileChunk) -> Void = { _ in }
) async -> UploadResult {
    await NetworkOperationCancellation(log: log).run(progress: progress) { cancellation in
        await performUpload(
            fileLocatedAt: localFilePath,
            toRemotePath: remotePath,
            sourceMetadata: sourceMetadata,
            usingRemoteInterface: remoteInterface,
            withAccount: account,
            inChunksSized: chunkSize,
            forItemWithIdentifier: itemIdentifier,
            dbManager: dbManager,
            cancellation: cancellation,
            creationDate: creationDate,
            modificationDate: modificationDate,
            options: options,
            creatingNewFile: creatingNewFile,
            log: log,
            requestHandler: { request in
                cancellation.register(request: request)
                requestHandler(request)
            },
            taskHandler: { task in
                cancellation.register(task: task)
                taskHandler(task)
            },
            progressHandler: progressHandler,
            chunkUploadCompleteHandler: chunkUploadCompleteHandler
        )
    }
}

private func performUpload(
    fileLocatedAt localFilePath: String,
    toRemotePath remotePath: String,
    sourceMetadata: SendableItemMetadata?,
    usingRemoteInterface remoteInterface: RemoteInterface,
    withAccount account: Account,
    inChunksSized chunkSize: Int?,
    forItemWithIdentifier itemIdentifier: String,
    dbManager: FilesDatabaseManager,
    cancellation: NetworkOperationCancellation,
    creationDate: Date?,
    modificationDate: Date?,
    options: NKRequestOptions,
    creatingNewFile: Bool,
    log: any FileProviderLogging,
    requestHandler: @Sendable @escaping (UploadRequest) -> Void,
    taskHandler: @Sendable @escaping (URLSessionTask) -> Void,
    progressHandler: @Sendable @escaping (Progress) -> Void,
    chunkUploadCompleteHandler: @Sendable @escaping (RemoteFileChunk) -> Void
) async -> UploadResult {
    let uploadLogger = FileProviderLogger(category: "upload", log: log)

    guard !cancellation.isCancelled, !Task.isCancelled else {
        return (nil, nil, nil, nil, .other(NKError(error: URLError(.cancelled))), nil)
    }

    let fileSize =
        (try? FileManager.default.attributesOfItem(atPath: localFilePath)[.size] as? Int64) ?? 0

    let createChecksum: String?
    do {
        createChecksum = creatingNewFile ? try CreateUploadChecksum.header(forFileAt: localFilePath, cancellation: cancellation) : nil
    } catch where cancellation.isCancelled || Task.isCancelled {
        return (nil, nil, nil, nil, .other(.cancelled), nil)
    } catch {
        return (nil, nil, nil, nil, .other(NKError(error: error)), nil)
    }

    let priorAttempt: PendingCreateUploadRecord? = if let createChecksum,
                                                      let pending = dbManager.pendingCreateUpload(itemIdentifier: itemIdentifier),
                                                      pending.checksum == createChecksum,
                                                      let pendingPath = account.filesRootRelativePath(for: pending.remotePath, sourceMetadata: sourceMetadata),
                                                      let destinationPath = account.filesRootRelativePath(for: remotePath, sourceMetadata: sourceMetadata),
                                                      pendingPath.utf8.elementsEqual(destinationPath.utf8)
    {
        pending
    } else {
        nil
    }

    func readDestination() async -> (metadata: SendableItemMetadata, responseBody: Data?)? {
        await remoteInterface.uploadDestination(
            at: remotePath,
            account: account,
            sourceMetadata: sourceMetadata,
            taskHandler: taskHandler
        )
    }

    func recoverCreate(
        from destination: (metadata: SendableItemMetadata, responseBody: Data?)?,
        recordedAttempt: PendingCreateUploadRecord? = nil
    ) -> UploadResult? {
        guard let priorAttempt,
              let (metadata, responseBody) = destination,
              !metadata.directory,
              metadata.size == fileSize,
              !metadata.etag.isEmpty,
              CreateUploadChecksum.matches(priorAttempt.checksum, metadata: metadata, responseBody: responseBody)
        else {
            return nil
        }
        uploadLogger.info("Recovered a file create with a matching checksum.", [.item: metadata.ocId, .url: remotePath])
        return (metadata.ocId, metadata.etag, metadata.date, metadata.size, nil, recordedAttempt ?? priorAttempt)
    }

    func resolveCollision(_ error: NKError, attempt: PendingCreateUploadRecord?) async -> UploadResult {
        let destination = await readDestination()
        guard !cancellation.isCancelled, !Task.isCancelled else {
            return (nil, nil, nil, nil, .other(.cancelled), nil)
        }
        if let recovered = recoverCreate(from: destination, recordedAttempt: attempt) {
            return recovered
        }
        return (nil, nil, nil, nil, .destinationCollision(error, metadata: destination?.metadata), attempt)
    }

    // The prior upload may already occupy the destination and consume the remaining quota.
    if priorAttempt != nil {
        let destination = await readDestination()
        guard !cancellation.isCancelled, !Task.isCancelled else {
            return (nil, nil, nil, nil, .other(.cancelled), nil)
        }
        if let recovered = recoverCreate(from: destination) {
            return recovered
        }
    }

    // Pre-flight quota check: a depth-0 PROPFIND on the destination's parent picks up
    // the user-level quota plus any tighter per-folder quota (group folders, external
    // shares). Failing fast here keeps multi-GB chunked uploads from running for minutes
    // before being rejected by a final 507. See nextcloud/desktop#9598.

    if let lastSlash = remotePath.lastIndex(of: "/") {
        let parentRemotePath = String(remotePath[..<lastSlash])

        if !parentRemotePath.isEmpty {
            let (_, files, _, propfindError) = await remoteInterface.enumerate(remotePath: parentRemotePath, depth: .target, showHiddenFiles: true, includeHiddenFiles: [], requestBody: nil, account: account, options: options, taskHandler: taskHandler)

            guard !cancellation.isCancelled, !Task.isCancelled else {
                return (nil, nil, nil, nil, .other(NKError(error: URLError(.cancelled))), nil)
            }

            if propfindError == .success, let availableBytes = files.first?.quotaAvailableBytes, availableBytes >= 0, fileSize > availableBytes {
                uploadLogger.info("Refusing upload: file size \(fileSize) bytes exceeds available server quota of \(availableBytes) bytes.", [.url: remotePath])

                let error = NKError(statusCode: 507, fallbackDescription: "Insufficient quota on the server.")
                return (nil, nil, nil, nil, .other(error), nil)
            } else if propfindError != .success {
                uploadLogger.info("Could not check available quota. Proceeding with upload anyway.", [.error: propfindError, .url: parentRemotePath])
            }
        }
    }

    let chunkSize = await {
        if let chunkSize {
            uploadLogger.info("Using provided chunkSize: \(chunkSize)")
            return chunkSize
        }

        let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(account: account)

        guard error == .success,
              let capabilities,
              let serverChunkSize = capabilities.files?.chunkedUpload?.maxChunkSize,
              serverChunkSize > 0
        else {
            uploadLogger.info(
                """
                Received nil capabilities data.
                    Received error: \(error.errorDescription)
                    Capabilities nil: \(capabilities == nil ? "YES" : "NO")
                    (if capabilities are not nil the server may just not provide chunk size data).
                    Using default file chunk size: \(defaultFileChunkSize)
                """
            )
            return defaultFileChunkSize
        }
        uploadLogger.info(
            """
            Received file chunk size from server: \(serverChunkSize)
            """
        )
        return Int(serverChunkSize)
    }()

    guard !cancellation.isCancelled, !Task.isCancelled else {
        return (nil, nil, nil, nil, .other(NKError(error: URLError(.cancelled))), nil)
    }

    func recordCreateAttempt() throws -> PendingCreateUploadRecord? {
        guard let createChecksum else { return nil }
        guard !cancellation.isCancelled, !Task.isCancelled else { throw CancellationError() }
        let attempt = PendingCreateUploadRecord(
            account: account.ncKitAccount,
            itemIdentifier: itemIdentifier,
            remotePath: remotePath,
            checksum: createChecksum
        )
        guard dbManager.recordPendingCreateUpload(attempt) else {
            throw NKError.metadataDatabaseWriteFailed
        }
        return attempt
    }

    var createAttempt: PendingCreateUploadRecord?
    guard fileSize > chunkSize else {
        let uploadOptions: NKRequestOptions
        if creatingNewFile {
            var headers = options.customHeader ?? [:]
            headers["If-None-Match"] = "*"
            do {
                createAttempt = try recordCreateAttempt()
            } catch let error as NKError {
                return (nil, nil, nil, nil, .other(error), nil)
            } catch {
                return (nil, nil, nil, nil, .other(.cancelled), nil)
            }
            if let createAttempt {
                headers["OC-Checksum"] = createAttempt.checksum
            }
            uploadOptions = NKRequestOptions(customHeader: headers, queue: options.queue)
        } else {
            uploadOptions = options
        }
        guard !cancellation.isCancelled, !Task.isCancelled else {
            if priorAttempt == nil, let createAttempt {
                dbManager.discardPendingCreateUpload(createAttempt)
            }
            return (nil, nil, nil, nil, .other(.cancelled), nil)
        }
        let (_, ocId, etag, date, size, _, remoteError) = await remoteInterface.upload(
            remotePath: remotePath,
            localPath: localFilePath,
            creationDate: creationDate,
            modificationDate: modificationDate,
            account: account,
            options: uploadOptions,
            requestHandler: requestHandler,
            taskHandler: taskHandler,
            progressHandler: progressHandler
        )

        if let createAttempt, priorAttempt == nil, remoteError != .success, !remoteError.isAmbiguousCreateResponse {
            dbManager.discardPendingCreateUpload(createAttempt)
        }
        if creatingNewFile, remoteError.isPreconditionFailedError {
            return await resolveCollision(remoteError, attempt: createAttempt)
        }
        let failure: UploadFailure? = remoteError == .success ? nil : .other(remoteError)
        return (ocId, etag, date as? Date, size, failure, createAttempt)
    }

    guard !cancellation.isCancelled, !Task.isCancelled else {
        return (nil, nil, nil, nil, .other(.cancelled), nil)
    }
    let chunkUploadId = chunkUploadIdentifier(
        forItemWithIdentifier: itemIdentifier,
        fileSize: fileSize,
        modificationDate: modificationDate,
        createDestination: creatingNewFile ? remotePath : nil
    )

    uploadLogger.info(
        """
        Performing chunked upload to \(remotePath)
            localFilePath: \(localFilePath)
            remoteChunkStoreFolderName: \(chunkUploadId)
            chunkSize: \(chunkSize)
        """
    )

    var completedChunksDirectory: URL?
    var shouldRemoveLocalChunkUpload = false
    defer {
        if shouldRemoveLocalChunkUpload {
            removeLocalChunkUpload(
                uploadIdentifier: chunkUploadId,
                chunksDirectory: completedChunksDirectory,
                usingRemoteInterface: remoteInterface,
                dbManager: dbManager,
                logger: uploadLogger
            )
        }
    }

    // Discard local chunks for older contents or create destinations. The lookup by the current
    // identifier below keeps stale rows out of this upload even if their cleanup fails.
    discardChunkUploads(
        forItemIdentifiers: [itemIdentifier],
        excluding: chunkUploadId,
        usingRemoteInterface: remoteInterface,
        dbManager: dbManager,
        logger: uploadLogger
    )

    setChunkUploadIdentifier(
        uploadIdentifier: chunkUploadId,
        itemIdentifier: itemIdentifier,
        dbManager: dbManager,
        logger: uploadLogger
    )

    // An empty list would start the upload from scratch and overwrite the recorded chunks; the identifier stays bound, so the next attempt resumes.
    guard let remainingChunks = dbManager.remoteFileChunks(uploadId: chunkUploadId) else {
        uploadLogger.error("Could not read the recorded chunks of the upload. The upload is retried later.", [.name: chunkUploadId])
        return (nil, nil, nil, nil, .other(.metadataDatabaseUnreadable), nil)
    }

    let (_, file, chunksDirectory, failure) = await remoteInterface.chunkedUpload(
        localPath: localFilePath,
        remotePath: remotePath,
        remoteChunkStoreFolderName: chunkUploadId,
        chunkSize: chunkSize,
        remainingChunks: remainingChunks,
        creationDate: creationDate,
        modificationDate: modificationDate,
        overwrite: !creatingNewFile,
        account: account,
        options: options,
        currentNumChunksUpdateHandler: { _ in },
        chunkCounter: { currentChunk in
            uploadLogger.info(
                """
                \(localFilePath) current chunk: \(currentChunk)
                """
            )
        },
        log: log,
        chunkUploadStartHandler: { chunks in
            uploadLogger.info("\(localFilePath) chunked upload starting...")

            // Do not add chunks to database if we have done this already
            guard remainingChunks.isEmpty else { return }

            if !dbManager.addRemoteFileChunks(chunks) {
                uploadLogger.error("Could not write chunks to db, won't be able to resume upload if transfer stops.")
            }
        },
        requestHandler: requestHandler,
        taskHandler: taskHandler,
        progressHandler: progressHandler,
        chunkUploadCompleteHandler: { chunk in
            uploadLogger.info(
                "\(localFilePath) chunk \(chunk.fileName) done"
            )
            if !dbManager.removeRemoteFileChunk(uploadId: chunkUploadId, fileName: chunk.fileName) {
                uploadLogger.error("Could not delete chunks in db, won't resume upload correctly if transfer stops.")
            }

            chunkUploadCompleteHandler(chunk)
        }
    )

    completedChunksDirectory = chunksDirectory
    let chunkUploadCompleted = failure == nil && file != nil
    let hasRemainingChunks = dbManager.hasRemoteFileChunks(uploadId: chunkUploadId)
    shouldRemoveLocalChunkUpload = chunkUploadCompleted || !hasRemainingChunks

    if let error = failure?.underlyingError,
       error.isCancelledError || error.isChunkingCancellationError,
       cancellation.isCancelled || Task.isCancelled
    {
        return (nil, nil, nil, nil, .other(.cancelled), nil)
    }

    if chunkUploadCompleted {
        uploadLogger.info("File successfully uploaded in chunks.", [.url: remotePath])
    }

    if case let .destinationCollision(error, _) = failure {
        return await resolveCollision(error, attempt: nil)
    }
    return (file?.ocId, file?.etag, file?.date, file?.size, failure, nil)
}
