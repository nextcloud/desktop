//
//  Upload.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-29.
//

import Alamofire
import Foundation
import NextcloudCapabilitiesKit
import NextcloudKit
import OSLog
import RealmSwift

let defaultFileChunkSize = 104_857_600 // 100 MiB
let uploadLogger = Logger(subsystem: Logger.subsystem, category: "upload")

func upload(
    fileLocatedAt localFilePath: String,
    toRemotePath remotePath: String,
    usingRemoteInterface remoteInterface: RemoteInterface,
    withAccount account: Account,
    inChunksSized chunkSize: Int? = nil,
    usingChunkUploadId chunkUploadId: String? = UUID().uuidString,
    dbManager: FilesDatabaseManager,
    creationDate: Date? = nil,
    modificationDate: Date? = nil,
    options: NKRequestOptions = .init(queue: .global(qos: .utility)),
    requestHandler: @escaping (UploadRequest) -> Void = { _ in },
    taskHandler: @escaping (URLSessionTask) -> Void = { _ in },
    progressHandler: @escaping (Progress) -> Void = { _ in },
    chunkUploadCompleteHandler: @escaping (_ fileChunk: RemoteFileChunk) -> Void  = { _ in }
) async -> (
    ocId: String?,
    chunks: [RemoteFileChunk]?,
    etag: String?,
    date: Date?,
    size: Int64?,
    afError: AFError?,
    remoteError: NKError
) {
    let fileSize =
        (try? FileManager.default.attributesOfItem(atPath: localFilePath)[.size] as? Int64) ?? 0

    let chunkSize = await {
        if let chunkSize {
            uploadLogger.info("Using provided chunkSize: \(chunkSize, privacy: .public)")
            return chunkSize
        }
        let (_, capabilities, _, error) = await remoteInterface.currentCapabilities(
            account: account, options: options, taskHandler: taskHandler
        )
        guard error == .success,
              let capabilities,
              let serverChunkSize = capabilities.files?.chunkedUpload?.maxChunkSize,
              serverChunkSize > 0
        else {
            uploadLogger.info(
                """
                Received nil capabilities data.
                    Received error: \(error.errorDescription, privacy: .public)
                    Capabilities nil: \(capabilities == nil ? "YES" : "NO", privacy: .public)
                    (if capabilities are not nil the server may just not provide chunk size data).
                    Using default file chunk size: \(defaultFileChunkSize, privacy: .public)
                """
            )
            return defaultFileChunkSize
        }
        uploadLogger.info(
            """
            Received file chunk size from server: \(serverChunkSize, privacy: .public)
            """
        )
        return Int(serverChunkSize)
    }()

    guard fileSize > chunkSize else {
        let (_, ocId, etag, date, size, _, afError, remoteError) = await remoteInterface.upload(
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

        return (ocId, nil, etag, date as? Date, size, afError, remoteError)
    }

    let chunkUploadId = chunkUploadId ?? UUID().uuidString

    uploadLogger.info(
        """
        Performing chunked upload to \(remotePath, privacy: .public)
            localFilePath: \(localFilePath, privacy: .public)
            remoteChunkStoreFolderName: \(chunkUploadId, privacy: .public)
            chunkSize: \(chunkSize, privacy: .public)
        """
    )

    let remainingChunks = dbManager
        .ncDatabase()
        .objects(RemoteFileChunk.self)
        .where({ $0.remoteChunkStoreFolderName == chunkUploadId })
        .toUnmanagedResults()

    let (_, chunks, file, afError, remoteError) = await remoteInterface.chunkedUpload(
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
                \(localFilePath, privacy: .public) current chunk: \(currentChunk, privacy: .public)
                """
            )
        },
        chunkUploadStartHandler: { chunks in
            uploadLogger.info("\(localFilePath, privacy: .public) chunked upload starting...")

            // Do not add chunks to database if we have done this already
            guard remainingChunks.isEmpty else { return }

            let db = dbManager.ncDatabase()
            do {
                try db.write { db.add(chunks.map { RemoteFileChunk(value: $0) }) }
            } catch let error {
                uploadLogger.error(
                    """
                    Could not write chunks to db, won't be able to resume upload if transfer stops.
                        \(error.localizedDescription, privacy: .public)
                    """
                )
            }
        },
        requestHandler: requestHandler,
        taskHandler: taskHandler,
        progressHandler: progressHandler,
        chunkUploadCompleteHandler: { chunk in
            uploadLogger.info(
                "\(localFilePath, privacy: .public) chunk \(chunk.fileName, privacy: .public) done"
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
                        .forEach { db.delete($0) } }
            } catch let error {
                uploadLogger.error(
                    """
                    Could not delete chunks in db, won't resume upload correctly if transfer stops.
                        \(error.localizedDescription, privacy: .public)
                    """
                )
            }
            chunkUploadCompleteHandler(chunk)
        }
    )

    uploadLogger.info("\(localFilePath, privacy: .public) successfully uploaded in chunks")

    return (file?.ocId, chunks, file?.etag, file?.date, file?.size, afError, remoteError)
}
