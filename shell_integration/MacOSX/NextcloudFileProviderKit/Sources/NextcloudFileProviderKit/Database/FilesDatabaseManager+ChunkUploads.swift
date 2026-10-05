//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import RealmSwift

///
/// Bookkeeping for chunked uploads: the chunks recorded per upload, the upload an item is currently bound to, and uploads whose local cleanup still has to be retried.
///
extension FilesDatabaseManager {
    /// The chunks recorded for the upload, in chunk order.
    func remoteFileChunks(uploadId: String) -> [RemoteFileChunk] {
        ncDatabase()
            .objects(LegacyRealmRemoteFileChunk.self)
            .where { $0.remoteChunkStoreFolderName == uploadId }
            .map(\.chunk)
            .sorted { (Int($0.fileName) ?? 0) < (Int($1.fileName) ?? 0) }
    }

    /// Record chunks for an upload. Returns `false` when nothing was written.
    @discardableResult
    func addRemoteFileChunks(_ chunks: [RemoteFileChunk]) -> Bool {
        let database = ncDatabase()

        do {
            try database.write {
                database.add(chunks.map { LegacyRealmRemoteFileChunk($0) })
            }
            return true
        } catch {
            logger.error("Could not record upload chunks.", [.error: error])
            return false
        }
    }

    /// Forget one recorded chunk of an upload. Returns `false` when the removal failed.
    @discardableResult
    func removeRemoteFileChunk(uploadId: String, fileName: String) -> Bool {
        let database = ncDatabase()

        do {
            try database.write {
                database.delete(
                    database
                        .objects(LegacyRealmRemoteFileChunk.self)
                        .where { $0.remoteChunkStoreFolderName == uploadId && $0.fileName == fileName }
                )
            }
            return true
        } catch {
            logger.error("Could not remove recorded upload chunk.", [.error: error, .name: uploadId])
            return false
        }
    }

    /// Whether any chunk is still recorded for the upload.
    func hasRemoteFileChunks(uploadId: String) -> Bool {
        !ncDatabase()
            .objects(LegacyRealmRemoteFileChunk.self)
            .where { $0.remoteChunkStoreFolderName == uploadId }
            .isEmpty
    }

    /// The upload identifier of every recorded chunk, one entry per chunk.
    func remoteChunkStoreFolderNames() -> [String] {
        ncDatabase()
            .objects(LegacyRealmRemoteFileChunk.self)
            .map(\.remoteChunkStoreFolderName)
    }

    /// Upload identifiers bound to any item, including deleted items and items in every status.
    func chunkUploadIdentifiers() -> [String] {
        itemMetadatas
            .where { $0.chunkUploadId != nil }
            .compactMap(\.chunkUploadId)
    }

    /// Upload identifiers bound to a live item whose upload is still in progress or failed, i.e. uploads which may be resumed.
    func resumableChunkUploadIdentifiers() -> [String] {
        itemMetadatas
            .where {
                $0.chunkUploadId != nil &&
                    $0.deleted == false &&
                    ($0.status == Status.inUpload.rawValue ||
                        $0.status == Status.uploading.rawValue ||
                        $0.status == Status.uploadError.rawValue)
            }
            .compactMap(\.chunkUploadId)
    }

    /// Upload identifiers whose local cleanup failed and has to be retried.
    func pendingChunkUploadCleanupIdentifiers() -> [String] {
        ncDatabase()
            .objects(RealmPendingChunkUploadCleanup.self)
            .map(\.uploadIdentifier)
    }

    /// Bind an item to an upload, or unbind it with `nil`. Returns `false` when the item is unknown or the write failed.
    @discardableResult
    func setChunkUploadIdentifier(_ uploadId: String?, ocId: String) -> Bool {
        let database = ncDatabase()

        guard let metadata = database.object(ofType: RealmItemMetadata.self, forPrimaryKey: ocId) else {
            return false
        }

        do {
            try database.write { metadata.chunkUploadId = uploadId }
            return true
        } catch {
            logger.error("Could not associate chunk upload with item metadata.", [.error: error, .item: ocId])
            return false
        }
    }

    /// Forget an upload entirely: its chunks, its pending cleanup marker and its binding to any item.
    @discardableResult
    func removeChunkUploadBookkeeping(uploadId: String) -> Bool {
        let database = ncDatabase()

        do {
            let chunks = database
                .objects(LegacyRealmRemoteFileChunk.self)
                .where { $0.remoteChunkStoreFolderName == uploadId }
            let owners = database
                .objects(RealmItemMetadata.self)
                .where { $0.chunkUploadId == uploadId }
            let pendingCleanup = database
                .objects(RealmPendingChunkUploadCleanup.self)
                .where { $0.uploadIdentifier == uploadId }

            try database.write {
                database.delete(chunks)
                database.delete(pendingCleanup)
                owners.forEach { $0.chunkUploadId = nil }
            }
            return true
        } catch {
            logger.error("Could not clear chunk upload bookkeeping.", [.error: error, .name: uploadId])
            return false
        }
    }

    /// Remember that the upload's local chunks still have to be removed.
    @discardableResult
    func recordPendingChunkUploadCleanup(uploadId: String) -> Bool {
        let database = ncDatabase()

        do {
            try database.write {
                database.add(RealmPendingChunkUploadCleanup(uploadIdentifier: uploadId), update: .modified)
            }
            return true
        } catch {
            logger.error("Could not record pending chunk upload cleanup.", [.error: error, .name: uploadId])
            return false
        }
    }
}
