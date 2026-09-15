//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import RealmSwift

/// Associates an in-progress chunk upload with existing item metadata.
func setChunkUploadIdentifier(
    uploadIdentifier: String,
    itemIdentifier: String,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    let db = dbManager.ncDatabase()
    guard let metadata = db.object(ofType: RealmItemMetadata.self, forPrimaryKey: itemIdentifier) else {
        return
    }

    do {
        try db.write { metadata.chunkUploadId = uploadIdentifier }
    } catch {
        logger.error("Could not associate chunk upload with item metadata.", [.error: error, .item: itemIdentifier])
    }
}

/// Removes tracked chunk uploads owned by the supplied items, optionally retaining one upload.
func discardChunkUploads(
    forItemIdentifiers itemIdentifiers: [String],
    excluding retainedChunkUploadIdentifier: String? = nil,
    usingRemoteInterface remoteInterface: RemoteInterface,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    let db = dbManager.ncDatabase()
    var uploadIdentifiers = Set<String>()

    for itemIdentifier in itemIdentifiers {
        if let uploadIdentifier = db.object(
            ofType: RealmItemMetadata.self,
            forPrimaryKey: itemIdentifier
        )?.chunkUploadId {
            uploadIdentifiers.insert(uploadIdentifier)
        }

        let itemPrefix = chunkUploadIdentifierPrefix(forItemWithIdentifier: itemIdentifier)
        let chunkIdentifiers = db.objects(RemoteFileChunk.self)
            .where { $0.remoteChunkStoreFolderName.starts(with: itemPrefix) }
            .map(\.remoteChunkStoreFolderName)
        uploadIdentifiers.formUnion(chunkIdentifiers)
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
    recordPendingChunkUploadCleanup(
        uploadIdentifier: uploadIdentifier,
        dbManager: dbManager,
        logger: logger
    )

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

    removeChunkUploadBookkeeping(
        uploadIdentifier: uploadIdentifier,
        dbManager: dbManager,
        logger: logger
    )
}

/// Removes tracked uploads that cannot be resumed after extension startup.
func cleanupAbandonedChunkUploads(
    usingRemoteInterface remoteInterface: RemoteInterface,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    let db = dbManager.ncDatabase()
    let chunkIdentifiers = db.objects(RemoteFileChunk.self)
        .map(\.remoteChunkStoreFolderName)
    let metadata = db.objects(RealmItemMetadata.self)
    let metadataUploadIdentifiers = metadata
        .where { $0.chunkUploadId != nil }
        .compactMap(\.chunkUploadId)
    let pendingCleanupIdentifiers = db.objects(RealmPendingChunkUploadCleanup.self)
        .map(\.uploadIdentifier)
    let knownIdentifiers = Set(chunkIdentifiers)
        .union(metadataUploadIdentifiers)
        .union(pendingCleanupIdentifiers)
    let resumableIdentifiers = Set(
        metadata
            .where {
                $0.chunkUploadId != nil &&
                    $0.deleted == false &&
                    ($0.status == Status.inUpload.rawValue ||
                        $0.status == Status.uploading.rawValue ||
                        $0.status == Status.uploadError.rawValue)
            }
            .compactMap(\.chunkUploadId)
    )

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

        removeChunkUploadBookkeeping(
            uploadIdentifier: uploadIdentifier,
            dbManager: dbManager,
            logger: logger
        )
    }
}

private func removeChunkUploadBookkeeping(
    uploadIdentifier: String,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    let db = dbManager.ncDatabase()

    do {
        let chunks = db.objects(RemoteFileChunk.self)
            .where { $0.remoteChunkStoreFolderName == uploadIdentifier }
        let owners = db.objects(RealmItemMetadata.self)
            .where { $0.chunkUploadId == uploadIdentifier }
        let pendingCleanup = db.objects(RealmPendingChunkUploadCleanup.self)
            .where { $0.uploadIdentifier == uploadIdentifier }
        try db.write {
            db.delete(chunks)
            db.delete(pendingCleanup)
            owners.forEach { $0.chunkUploadId = nil }
        }
    } catch {
        logger.error(
            "Could not clear chunk upload bookkeeping.",
            [.error: error, .name: uploadIdentifier]
        )
    }
}

private func recordPendingChunkUploadCleanup(
    uploadIdentifier: String,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
    let db = dbManager.ncDatabase()

    do {
        try db.write {
            db.add(
                RealmPendingChunkUploadCleanup(uploadIdentifier: uploadIdentifier),
                update: .modified
            )
        }
    } catch {
        logger.error(
            "Could not record pending chunk upload cleanup.",
            [.error: error, .name: uploadIdentifier]
        )
    }
}
