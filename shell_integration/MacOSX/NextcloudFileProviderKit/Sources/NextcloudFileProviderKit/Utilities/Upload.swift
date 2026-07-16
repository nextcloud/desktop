//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit
import RealmSwift

let defaultFileChunkSize = 104_857_600 // 100 MiB

/// The per-item prefix shared by every chunked-upload identifier for a given item, so stale chunk
/// bookkeeping from earlier versions of the same item can be swept.
func chunkUploadIdentifierPrefix(forItemWithIdentifier itemIdentifier: String) -> String {
    itemIdentifier.replacingOccurrences(of: "/", with: "") + "_"
}

/// Derives a stable, *content-scoped* identifier for a chunked upload's server folder and its local
/// `RemoteFileChunk` bookkeeping.
///
/// The identity is `(item, size, modificationDate)`: the same content re-uploads under the same id,
/// so an interrupted transfer resumes and reuses the chunks already stored on the server. Any content
/// change yields a different id, so chunks from a previous version are never spliced into a different
/// one (see F3). The value is prefixed with the item id so stale sets can be swept by prefix.
///
/// When no modification date is available the content can't be bound to an id, so a per-attempt unique
/// id is used: this forgoes resume for that upload but never risks a bad splice. In the File Provider
/// model a real content change always bumps `contentModificationDate` (that is how the change was
/// detected), so equal `(size, modificationDate)` reliably means identical content.
func chunkUploadIdentifier(
    forItemWithIdentifier itemIdentifier: String, fileSize: Int64, modificationDate: Date?
) -> String {
    let prefix = chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)

    guard let modificationDate else {
        return prefix + UUID().uuidString
    }

    let mtimeSeconds = Int64(modificationDate.timeIntervalSince1970.rounded())
    return "\(prefix)\(fileSize)_\(mtimeSeconds)"
}

func upload(
    fileLocatedAt localFilePath: String,
    toRemotePath remotePath: String,
    usingRemoteInterface remoteInterface: RemoteInterface,
    withAccount account: Account,
    inChunksSized chunkSize: Int? = nil,
    forItemWithIdentifier itemIdentifier: String,
    dbManager: FilesDatabaseManager,
    creationDate: Date? = nil,
    modificationDate: Date? = nil,
    options: NKRequestOptions = .init(queue: .global(qos: .utility)),
    log: any FileProviderLogging,
    requestHandler: @escaping (UploadRequest) -> Void = { _ in },
    taskHandler: @Sendable @escaping (URLSessionTask) -> Void = { _ in },
    progressHandler: @escaping (Progress) -> Void = { _ in },
    chunkUploadCompleteHandler: @escaping (_ fileChunk: RemoteFileChunk) -> Void = { _ in }
) async -> (
    ocId: String?,
    etag: String?,
    date: Date?,
    size: Int64?,
    remoteError: NKError
) {
    let uploadLogger = FileProviderLogger(category: "upload", log: log)

    let fileSize =
        (try? FileManager.default.attributesOfItem(atPath: localFilePath)[.size] as? Int64) ?? 0

    // Pre-flight quota check: a depth-0 PROPFIND on the destination's parent picks up
    // the user-level quota plus any tighter per-folder quota (group folders, external
    // shares). Failing fast here keeps multi-GB chunked uploads from running for minutes
    // before being rejected by a final 507. See nextcloud/desktop#9598.

    if let lastSlash = remotePath.lastIndex(of: "/") {
        let parentRemotePath = String(remotePath[..<lastSlash])

        if !parentRemotePath.isEmpty {
            let (_, files, _, propfindError) = await remoteInterface.enumerate(remotePath: parentRemotePath, depth: .target, showHiddenFiles: true, includeHiddenFiles: [], requestBody: nil, account: account, options: options, taskHandler: taskHandler)

            if propfindError == .success, let availableBytes = files.first?.quotaAvailableBytes, availableBytes >= 0, fileSize > availableBytes {
                uploadLogger.info("Refusing upload: file size \(fileSize) bytes exceeds available server quota of \(availableBytes) bytes.", [.url: remotePath])

                return (nil, nil, nil, nil, NKError(statusCode: 507, fallbackDescription: "Insufficient quota on the server."))
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

        let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(account: account, options: options, taskHandler: taskHandler)

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

    guard fileSize > chunkSize else {
        let (_, ocId, etag, date, size, _, remoteError) = await remoteInterface.upload(
            remotePath: remotePath,
            localPath: localFilePath,
            creationDate: creationDate,
            modificationDate: modificationDate,
            account: account,
            options: options,
            requestHandler: requestHandler,
            taskHandler: taskHandler,
            progressHandler: progressHandler
        )

        return (ocId, etag, date as? Date, size, remoteError)
    }

    let chunkUploadId = chunkUploadIdentifier(
        forItemWithIdentifier: itemIdentifier, fileSize: fileSize, modificationDate: modificationDate
    )

    uploadLogger.info(
        """
        Performing chunked upload to \(remotePath)
            localFilePath: \(localFilePath)
            remoteChunkStoreFolderName: \(chunkUploadId)
            chunkSize: \(chunkSize)
        """
    )

    // Content-scoped resume (F3): drop any chunk bookkeeping left over from a *different* version of
    // this same item — a prior interrupted upload whose content has since changed derives a different
    // id. Keeping those rows would risk resuming against server-side chunks that belong to the old
    // content and splicing them into the new file. Rows under the current id (a genuine resume of
    // identical content) share the id and are preserved.
    let staleChunkPrefix = chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)
    do {
        let db = dbManager.ncDatabase()
        let staleChunks = db.objects(RemoteFileChunk.self).where {
            $0.remoteChunkStoreFolderName.starts(with: staleChunkPrefix)
                && $0.remoteChunkStoreFolderName != chunkUploadId
        }

        if !staleChunks.isEmpty {
            try db.write { db.delete(staleChunks) }
        }
    } catch {
        uploadLogger.error("Could not clear stale chunk bookkeeping for item.", [.error: error])
    }

    let remainingChunks = dbManager
        .ncDatabase()
        .objects(RemoteFileChunk.self)
        .where { $0.remoteChunkStoreFolderName == chunkUploadId }
        .toUnmanagedResults()

    let (_, file, nkError) = await remoteInterface.chunkedUpload(
        localPath: localFilePath,
        remotePath: remotePath,
        remoteChunkStoreFolderName: chunkUploadId,
        chunkSize: chunkSize,
        remainingChunks: remainingChunks,
        creationDate: creationDate,
        modificationDate: modificationDate,
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

            let db = dbManager.ncDatabase()
            do {
                try db.write { db.add(chunks.map { RemoteFileChunk(value: $0) }) }
            } catch {
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
            let db = dbManager.ncDatabase()
            do {
                try db.write {
                    db
                        .objects(RemoteFileChunk.self)
                        .where {
                            $0.remoteChunkStoreFolderName == chunkUploadId &&
                                $0.fileName == chunk.fileName
                        }
                        .forEach { db.delete($0) }
                }
            } catch {
                uploadLogger.error("Could not delete chunks in db, won't resume upload correctly if transfer stops.", [.error: error])
            }

            chunkUploadCompleteHandler(chunk)
        }
    )

    uploadLogger.info("\(localFilePath) successfully uploaded in chunks")

    return (file?.ocId, file?.etag, file?.date, file?.size, nkError)
}
