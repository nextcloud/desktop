//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import RealmSwift

/// Removes one local chunk upload and its existing bookkeeping.
func removeLocalChunkUpload(
    uploadIdentifier: String,
    chunksDirectory: URL?,
    usingRemoteInterface remoteInterface: RemoteInterface,
    dbManager: FilesDatabaseManager,
    logger: FileProviderLogger
) {
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
        try db.write {
            db.delete(chunks)
            owners.forEach { $0.chunkUploadId = nil }
        }
    } catch {
        logger.error(
            "Could not clear chunk upload bookkeeping.",
            [.error: error, .name: uploadIdentifier]
        )
    }
}
