//
//  RemoteFileChunk.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2025-01-08.
//

import Foundation
import NextcloudKit
import RealmSwift

public class RemoteFileChunk: Object {
    @Persisted public var fileName: String
    @Persisted public var size: Int64
    @Persisted public var remoteChunkStoreFolderName: String

    static func fromNcKitChunks(
        _ chunks: [(fileName: String, size: Int64)], remoteChunkStoreFolderName: String
    ) -> [RemoteFileChunk] {
        chunks.map {
            RemoteFileChunk(ncKitChunk: $0, remoteChunkStoreFolderName: remoteChunkStoreFolderName)
        }
    }

    convenience init(ncKitChunk: (fileName: String, size: Int64), remoteChunkStoreFolderName: String) {
        self.init(
            fileName: ncKitChunk.fileName,
            size: ncKitChunk.size,
            remoteChunkStoreFolderName: remoteChunkStoreFolderName
        )
    }

    convenience init(fileName: String, size: Int64, remoteChunkStoreFolderName: String) {
        self.init()
        self.fileName = fileName
        self.size = size
        self.remoteChunkStoreFolderName = remoteChunkStoreFolderName
    }

    func toNcKitChunk() -> (fileName: String, size: Int64) {
        (fileName, size)
    }
}

extension Array<RemoteFileChunk> {
    func toNcKitChunks() -> [(fileName: String, size: Int64)] {
        map { ($0.fileName, $0.size) }
    }
}
