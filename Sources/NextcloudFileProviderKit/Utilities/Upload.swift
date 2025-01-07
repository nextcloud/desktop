//
//  Upload.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-29.
//

import Alamofire
import Foundation
import NextcloudKit
import OSLog

let defaultFileChunkSize = 10_000_000 // 10 MB
let uploadLogger = Logger(subsystem: Logger.subsystem, category: "upload")

struct UploadResult {
    let ocId: String?
    let chunks: [RemoteFileChunk]?
    let etag: String?
    let date: Date?
    let size: Int64?
    let afError: AFError?
    let remoteError: NKError
    var succeeded: Bool { remoteError == .success }
}

func upload(
    fileLocatedAt localFileUrl: URL,
    toRemotePath remotePath: String,
    usingRemoteInterface remoteInterface: RemoteInterface,
    withAccount account: Account,
    inChunksSized chunkSize: Int = defaultFileChunkSize,
    usingChunkUploadId chunkUploadId: String = UUID().uuidString,
    creationDate: Date? = nil,
    modificationDate: Date? = nil,
    options: NKRequestOptions = .init(),
    requestHandler: @escaping (UploadRequest) -> Void = { _ in },
    taskHandler: @escaping (URLSessionTask) -> Void = { _ in },
    progressHandler: @escaping (Progress) -> Void = { _ in }
) async -> UploadResult {
    let localPath = localFileUrl.path
    let fileSize =
        (try? FileManager.default.attributesOfItem(atPath: localPath)[.size] as? Int64) ?? 0
    guard fileSize > chunkSize else {
        let (_, ocId, etag, date, size, _, afError, remoteError) = await remoteInterface.upload(
            remotePath: remotePath,
            localPath: localFileUrl.path,
            creationDate: creationDate,
            modificationDate: modificationDate,
            account: account,
            options: options,
            requestHandler: requestHandler,
            taskHandler: taskHandler,
            progressHandler: progressHandler
        )

        return UploadResult(
            ocId: ocId,
            chunks: nil,
            etag: etag,
            date: date as? Date,
            size: size,
            afError: afError,
            remoteError: remoteError
        )
    }

    let localFileName = localFileUrl.lastPathComponent
    let localParentDirectoryPath = localFileUrl.deletingLastPathComponent().path
    let remoteParentDirectoryPath = (remotePath as NSString).deletingLastPathComponent as String
    // TODO: Pick up where left off for past failed chunked uploads
    let (_, chunks, file, afError, remoteError) = await remoteInterface.chunkedUpload(
        localDirectoryPath: localParentDirectoryPath,
        localFileName: localFileName,
        remoteParentDirectoryPath: remoteParentDirectoryPath,
        remoteChunkStoreFolderName: chunkUploadId,
        chunkSize: chunkSize,
        remainingChunks: [],
        creationDate: creationDate,
        modificationDate: modificationDate,
        account: account,
        options: options,
        currentNumChunksUpdateHandler: { _ in },
        chunkCounter: { currentChunk in
            uploadLogger.info(
                """
                \(localFileName, privacy: .public) current chunk: \(currentChunk, privacy: .public)
                """
            )
        },
        chunkUploadStartHandler: { _ in
            uploadLogger.info("\(localFileName, privacy: .public) uploading chunk")
        },
        requestHandler: requestHandler,
        taskHandler: taskHandler,
        progressHandler: progressHandler,
        chunkUploadCompleteHandler: { uploadedChunk in
            uploadLogger.info(
                """
                \(localFileName, privacy: .public) uploaded chunk:
                    \(uploadedChunk.fileName, privacy: .public)
                    (\(uploadedChunk.size, privacy: .public))
                """
            )
        }
    )

    return UploadResult(
        ocId: file?.name,
        chunks: chunks,
        etag: file?.etag,
        date: file?.date,
        size: file?.size,
        afError: afError,
        remoteError: remoteError
    )
}
