//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for the chunked-upload bookkeeping of ``FilesDatabaseManager``.
    ///
    @Suite("Chunk uploads")
    struct FilesDatabaseManagerChunkUploadTests {
        let manager = DatabaseTestSuites.makeManager()

        private func chunk(_ number: Int, uploadId: String, size: Int64 = 10) -> RemoteFileChunk {
            RemoteFileChunk(fileName: String(number), size: size, remoteChunkStoreFolderName: uploadId)
        }

        private func file(ocId: String, chunkUploadId: String?, status: Status = .normal, deleted: Bool = false) -> SendableItemMetadata {
            var metadata = DatabaseTestSuites.makeFile(ocId: ocId, fileName: "\(ocId).bin")
            metadata.chunkUploadId = chunkUploadId
            metadata.status = status.rawValue
            metadata.deleted = deleted
            return metadata
        }

        @Test func chunksAreReturnedInChunkOrderRegardlessOfInsertionOrder() {
            #expect(manager.addRemoteFileChunks([chunk(3, uploadId: "u"), chunk(1, uploadId: "u"), chunk(10, uploadId: "u"), chunk(2, uploadId: "u")]))

            #expect(manager.remoteFileChunks(uploadId: "u").map(\.fileName) == ["1", "2", "3", "10"])
        }

        @Test func chunksAreScopedToTheirUpload() {
            manager.addRemoteFileChunks([chunk(1, uploadId: "a"), chunk(2, uploadId: "a"), chunk(1, uploadId: "b")])

            #expect(manager.remoteFileChunks(uploadId: "a").count == 2)
            #expect(manager.remoteFileChunks(uploadId: "b").map(\.size) == [10])
            #expect(manager.remoteFileChunks(uploadId: "c").isEmpty)
        }

        @Test func removingOneChunkLeavesTheOthers() {
            manager.addRemoteFileChunks([chunk(1, uploadId: "a"), chunk(2, uploadId: "a"), chunk(2, uploadId: "b")])

            #expect(manager.removeRemoteFileChunk(uploadId: "a", fileName: "2"))

            #expect(manager.remoteFileChunks(uploadId: "a").map(\.fileName) == ["1"])
            #expect(manager.remoteFileChunks(uploadId: "b").map(\.fileName) == ["2"])
        }

        @Test func hasRemoteFileChunksFollowsTheRecordedRows() {
            #expect(manager.hasRemoteFileChunks(uploadId: "a") == false)

            manager.addRemoteFileChunks([chunk(1, uploadId: "a")])
            #expect(manager.hasRemoteFileChunks(uploadId: "a"))

            manager.removeRemoteFileChunk(uploadId: "a", fileName: "1")
            #expect(manager.hasRemoteFileChunks(uploadId: "a") == false)
        }

        @Test func folderNamesListEveryRecordedUpload() {
            manager.addRemoteFileChunks([chunk(1, uploadId: "prefix_a"), chunk(2, uploadId: "prefix_a"), chunk(1, uploadId: "other")])

            #expect(Set(manager.remoteChunkStoreFolderNames()) == ["prefix_a", "other"])
        }

        @Test func uploadIdentifiersBoundToItemsIncludeEveryStatusWhileResumableOnesDoNot() {
            manager.addItemMetadata(file(ocId: "settled", chunkUploadId: "u-settled"))
            manager.addItemMetadata(file(ocId: "in-upload", chunkUploadId: "u-in-upload", status: .inUpload))
            manager.addItemMetadata(file(ocId: "uploading", chunkUploadId: "u-uploading", status: .uploading))
            manager.addItemMetadata(file(ocId: "failed", chunkUploadId: "u-failed", status: .uploadError))
            manager.addItemMetadata(file(ocId: "deleted", chunkUploadId: "u-deleted", status: .inUpload, deleted: true))
            manager.addItemMetadata(file(ocId: "unbound", chunkUploadId: nil, status: .inUpload))

            #expect(Set(manager.chunkUploadIdentifiers()) == ["u-settled", "u-in-upload", "u-uploading", "u-failed", "u-deleted"])
            #expect(Set(manager.resumableChunkUploadIdentifiers()) == ["u-in-upload", "u-uploading", "u-failed"])
        }

        @Test func bindingAnUploadToAnUnknownItemFails() {
            #expect(manager.setChunkUploadIdentifier("u", ocId: "missing") == false)
        }

        @Test func bindingAndUnbindingAnUploadUpdatesTheItem() {
            manager.addItemMetadata(file(ocId: "item", chunkUploadId: nil))

            #expect(manager.setChunkUploadIdentifier("u", ocId: "item"))
            #expect(manager.itemMetadata(ocId: "item")?.chunkUploadId == "u")

            #expect(manager.setChunkUploadIdentifier(nil, ocId: "item"))
            #expect(manager.itemMetadata(ocId: "item")?.chunkUploadId == nil)
        }

        @Test func pendingCleanupMarkersAreRecordedOnce() {
            #expect(manager.recordPendingChunkUploadCleanup(uploadId: "u"))
            #expect(manager.recordPendingChunkUploadCleanup(uploadId: "u"))
            #expect(manager.recordPendingChunkUploadCleanup(uploadId: "v"))

            #expect(manager.pendingChunkUploadCleanupIdentifiers().sorted() == ["u", "v"])
        }

        @Test func removingBookkeepingClearsChunksMarkerAndOwners() {
            manager.addItemMetadata(file(ocId: "owner", chunkUploadId: "u", status: .inUpload))
            manager.addItemMetadata(file(ocId: "other-owner", chunkUploadId: "v", status: .inUpload))
            manager.addRemoteFileChunks([chunk(1, uploadId: "u"), chunk(1, uploadId: "v")])
            manager.recordPendingChunkUploadCleanup(uploadId: "u")
            manager.recordPendingChunkUploadCleanup(uploadId: "v")

            #expect(manager.removeChunkUploadBookkeeping(uploadId: "u"))

            #expect(manager.remoteFileChunks(uploadId: "u").isEmpty)
            #expect(manager.remoteFileChunks(uploadId: "v").count == 1)
            #expect(manager.pendingChunkUploadCleanupIdentifiers() == ["v"])
            #expect(manager.itemMetadata(ocId: "owner")?.chunkUploadId == nil)
            #expect(manager.itemMetadata(ocId: "other-owner")?.chunkUploadId == "v")
        }
    }
}
