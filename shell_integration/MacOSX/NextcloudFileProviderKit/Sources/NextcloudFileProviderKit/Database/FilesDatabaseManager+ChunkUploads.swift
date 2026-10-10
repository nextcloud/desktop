//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

///
/// Bookkeeping for chunked uploads: the chunks recorded per upload, the upload an item is currently bound to, and uploads whose local cleanup still has to be retried.
///
extension FilesDatabaseManager {
    /// The chunks recorded for the upload, in chunk order.
    func remoteFileChunks(uploadId: String) -> [RemoteFileChunk] {
        read("Could not fetch the recorded upload chunks.", [.name: uploadId]) { db in
            try RemoteFileChunkRecord
                .filter(RemoteFileChunkRecord.Columns.remoteChunkStoreFolderName == uploadId)
                .order(sql: "CAST(fileName AS INTEGER)")
                .fetchAll(db)
                .map(\.chunk)
        } ?? []
    }

    /// Record chunks for an upload. Returns `false` when nothing was written.
    @discardableResult
    func addRemoteFileChunks(_ chunks: [RemoteFileChunk]) -> Bool {
        write("Could not record upload chunks.") { db in
            for chunk in chunks {
                try RemoteFileChunkRecord(chunk).upsert(db)
            }
            return true
        } ?? false
    }

    /// Forget one recorded chunk of an upload. Returns `false` when the removal failed.
    @discardableResult
    func removeRemoteFileChunk(uploadId: String, fileName: String) -> Bool {
        write("Could not remove recorded upload chunk.", [.name: uploadId]) { db in
            try RemoteFileChunkRecord
                .filter(
                    RemoteFileChunkRecord.Columns.remoteChunkStoreFolderName == uploadId
                        && RemoteFileChunkRecord.Columns.fileName == fileName
                )
                .deleteAll(db)
            return true
        } ?? false
    }

    /// Whether any chunk is still recorded for the upload. Assumes chunks remain when the database cannot be read, so local chunks are never discarded on a lookup failure.
    func hasRemoteFileChunks(uploadId: String) -> Bool {
        read("Could not look up the recorded upload chunks.", [.name: uploadId]) { db in
            try !RemoteFileChunkRecord
                .filter(RemoteFileChunkRecord.Columns.remoteChunkStoreFolderName == uploadId)
                .isEmpty(db)
        } ?? true
    }

    /// The upload identifier of every recorded chunk, one entry per chunk.
    func remoteChunkStoreFolderNames() -> [String] {
        read("Could not fetch the recorded upload identifiers.") { db in
            try RemoteFileChunkRecord
                .select(RemoteFileChunkRecord.Columns.remoteChunkStoreFolderName, as: String.self)
                .fetchAll(db)
        } ?? []
    }

    /// Upload identifiers bound to any item, including deleted items and items in every status.
    func chunkUploadIdentifiers() -> [String] {
        read("Could not fetch the upload identifiers bound to items.") { db in
            try ItemMetadataRecord
                .filter(ItemMetadataRecord.Columns.chunkUploadId != nil)
                .select(ItemMetadataRecord.Columns.chunkUploadId, as: String.self)
                .fetchAll(db)
        } ?? []
    }

    /// Upload identifiers bound to a live item whose upload is still in progress or failed, i.e. uploads which may be resumed.
    func resumableChunkUploadIdentifiers() -> [String] {
        read("Could not fetch the resumable upload identifiers.") { db in
            try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.Columns.chunkUploadId != nil
                        && ItemMetadataRecord.Columns.deleted == false
                        && [Status.inUpload.rawValue, Status.uploading.rawValue, Status.uploadError.rawValue].contains(ItemMetadataRecord.Columns.status)
                )
                .select(ItemMetadataRecord.Columns.chunkUploadId, as: String.self)
                .fetchAll(db)
        } ?? []
    }

    /// Every known upload identifier which no live, unfinished item can resume, decided in one read so a partial failure cannot classify a resumable upload as abandoned. `nil` when the database could not be read.
    func abandonedChunkUploadIdentifiers() -> Set<String>? {
        read("Could not decide which chunk uploads are abandoned.") { db in
            let recorded = try RemoteFileChunkRecord
                .select(RemoteFileChunkRecord.Columns.remoteChunkStoreFolderName, as: String.self)
                .fetchAll(db)
            let bound = try ItemMetadataRecord
                .filter(ItemMetadataRecord.Columns.chunkUploadId != nil)
                .select(ItemMetadataRecord.Columns.chunkUploadId, as: String.self)
                .fetchAll(db)
            let pending = try PendingChunkUploadCleanupRecord.fetchAll(db).map(\.uploadIdentifier)
            let resumable = try ItemMetadataRecord
                .filter(
                    ItemMetadataRecord.Columns.chunkUploadId != nil
                        && ItemMetadataRecord.Columns.deleted == false
                        && [Status.inUpload.rawValue, Status.uploading.rawValue, Status.uploadError.rawValue].contains(ItemMetadataRecord.Columns.status)
                )
                .select(ItemMetadataRecord.Columns.chunkUploadId, as: String.self)
                .fetchAll(db)

            return Set(recorded).union(bound).union(pending).subtracting(resumable)
        }
    }

    /// Upload identifiers whose local cleanup failed and has to be retried.
    func pendingChunkUploadCleanupIdentifiers() -> [String] {
        read("Could not fetch the pending chunk upload cleanups.") { db in
            try PendingChunkUploadCleanupRecord.fetchAll(db).map(\.uploadIdentifier)
        } ?? []
    }

    /// Bind an item to an upload, or unbind it with `nil`. Returns `false` when the item is unknown or the write failed.
    @discardableResult
    func setChunkUploadIdentifier(_ uploadId: String?, ocId: String) -> Bool {
        write("Could not associate chunk upload with item metadata.", [.item: ocId]) { db in
            try ItemMetadataRecord
                .filter(key: ocId)
                .updateAll(db, ItemMetadataRecord.Columns.chunkUploadId.set(to: uploadId)) > 0
        } ?? false
    }

    /// Forget an upload entirely: its chunks, its pending cleanup marker and its binding to any item.
    @discardableResult
    func removeChunkUploadBookkeeping(uploadId: String) -> Bool {
        write("Could not clear chunk upload bookkeeping.", [.name: uploadId]) { db in
            try RemoteFileChunkRecord
                .filter(RemoteFileChunkRecord.Columns.remoteChunkStoreFolderName == uploadId)
                .deleteAll(db)
            _ = try PendingChunkUploadCleanupRecord.deleteOne(db, key: uploadId)
            try ItemMetadataRecord
                .filter(ItemMetadataRecord.Columns.chunkUploadId == uploadId)
                .updateAll(db, ItemMetadataRecord.Columns.chunkUploadId.set(to: nil))
            return true
        } ?? false
    }

    /// Remember that the upload's local chunks still have to be removed.
    @discardableResult
    func recordPendingChunkUploadCleanup(uploadId: String) -> Bool {
        write("Could not record pending chunk upload cleanup.", [.name: uploadId]) { db in
            try PendingChunkUploadCleanupRecord(uploadIdentifier: uploadId).upsert(db)
            return true
        } ?? false
    }
}
