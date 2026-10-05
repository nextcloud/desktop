//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

/// Associates an in-progress chunk upload with existing item metadata.
func setChunkUploadIdentifier(
    uploadIdentifier: String,
    itemIdentifier: String,
    dbManager: FilesDatabaseManager,
    logger _: FileProviderLogger
) {
    dbManager.setChunkUploadIdentifier(uploadIdentifier, ocId: itemIdentifier)
}

/// Removes tracked chunk uploads owned by the supplied items, optionally retaining one upload.
func discardChunkUploads(
    forItemIdentifiers itemIdentifiers: [String],
    excluding retainedChunkUploadIdentifier: String? = nil,
    usingRemoteInterface remoteInterface: RemoteInterface,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    let recordedUploadIdentifiers = dbManager.remoteChunkStoreFolderNames()
    var uploadIdentifiers = Set<String>()

    for itemIdentifier in itemIdentifiers {
        if let uploadIdentifier = dbManager.itemMetadata(ocId: itemIdentifier)?.chunkUploadId {
            uploadIdentifiers.insert(uploadIdentifier)
        }

        let itemPrefix = chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)
        uploadIdentifiers.formUnion(recordedUploadIdentifiers.filter { $0.hasPrefix(itemPrefix) })
    }

    if let retainedChunkUploadIdentifier {
        uploadIdentifiers.remove(retainedChunkUploadIdentifier)
    }

    discardChunkUploads(
        withIdentifiers: uploadIdentifiers,
        usingRemoteInterface: remoteInterface,
        dbManager: dbManager,
        logger: logger
    )
}

/// Removes one local chunk upload and its existing bookkeeping.
func removeLocalChunkUpload(
    uploadIdentifier: String,
    chunksDirectory: URL?,
    usingRemoteInterface remoteInterface: RemoteInterface,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    dbManager.recordPendingChunkUploadCleanup(uploadId: uploadIdentifier)

    do {
        if let chunksDirectory {
            do {
                try FileManager.default.removeItem(at: chunksDirectory)
            } catch CocoaError.fileNoSuchFile {
                // Nothing remains to clean up.
            }
        } else {
            try remoteInterface.removeLocalChunks(remoteChunkStoreFolderName: uploadIdentifier)
        }
    } catch {
        logger.error(
            "Could not remove local upload chunks.",
            [.error: error, .name: uploadIdentifier]
        )
        return
    }

    dbManager.removeChunkUploadBookkeeping(uploadId: uploadIdentifier)
}

/// Removes tracked uploads that cannot be resumed after extension startup.
func cleanupAbandonedChunkUploads(
    usingRemoteInterface remoteInterface: RemoteInterface,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    let knownIdentifiers = Set(dbManager.remoteChunkStoreFolderNames())
        .union(dbManager.chunkUploadIdentifiers())
        .union(dbManager.pendingChunkUploadCleanupIdentifiers())
    let resumableIdentifiers = Set(dbManager.resumableChunkUploadIdentifiers())

    discardChunkUploads(
        withIdentifiers: knownIdentifiers.subtracting(resumableIdentifiers),
        usingRemoteInterface: remoteInterface,
        dbManager: dbManager,
        logger: logger
    )
}

private func discardChunkUploads(
    withIdentifiers uploadIdentifiers: Set<String>,
    usingRemoteInterface remoteInterface: RemoteInterface,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    for uploadIdentifier in uploadIdentifiers {
        do {
            try remoteInterface.removeLocalChunks(remoteChunkStoreFolderName: uploadIdentifier)
        } catch {
            logger.error(
                "Could not remove abandoned local chunks.",
                [.error: error, .name: uploadIdentifier]
            )
            continue
        }

        dbManager.removeChunkUploadBookkeeping(uploadId: uploadIdentifier)
    }
}
