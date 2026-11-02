//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudKit

///
/// One chunk of a chunked upload as recorded in the file provider domain's database.
///
/// `fileName` is the chunk's number on the server, `size` its length in bytes and `remoteChunkStoreFolderName` the identifier of the upload it belongs to.
///
public struct RemoteFileChunk: Sendable, Hashable, Codable {
    public var fileName: String
    public var size: Int64
    public var remoteChunkStoreFolderName: String

    public init(fileName: String, size: Int64, remoteChunkStoreFolderName: String) {
        self.fileName = fileName
        self.size = size
        self.remoteChunkStoreFolderName = remoteChunkStoreFolderName
    }

    public init(ncKitChunk: (fileName: String, size: Int64), remoteChunkStoreFolderName: String) {
        self.init(
            fileName: ncKitChunk.fileName,
            size: ncKitChunk.size,
            remoteChunkStoreFolderName: remoteChunkStoreFolderName
        )
    }

    public static func fromNcKitChunks(
        _ chunks: [(fileName: String, size: Int64)], remoteChunkStoreFolderName: String
    ) -> [RemoteFileChunk] {
        chunks.map {
            RemoteFileChunk(ncKitChunk: $0, remoteChunkStoreFolderName: remoteChunkStoreFolderName)
        }
    }

    func toNcKitChunk() -> (fileName: String, size: Int64) {
        (fileName, size)
    }
}

extension [RemoteFileChunk] {
    func toNcKitChunks() -> [(fileName: String, size: Int64)] {
        map { ($0.fileName, $0.size) }
    }
}
