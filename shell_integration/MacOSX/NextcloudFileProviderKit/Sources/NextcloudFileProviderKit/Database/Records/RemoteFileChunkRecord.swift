//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import GRDB

/// Row of one recorded upload chunk, keyed by upload and chunk number.
struct RemoteFileChunkRecord: Codable, Sendable, FetchableRecord, PersistableRecord {
    static let databaseTableName = "remoteFileChunk"

    typealias Columns = CodingKeys

    enum CodingKeys: String, CodingKey, ColumnExpression {
        case remoteChunkStoreFolderName, fileName, size
    }

    var remoteChunkStoreFolderName: String
    var fileName: String
    var size: Int64

    init(_ chunk: RemoteFileChunk) {
        remoteChunkStoreFolderName = chunk.remoteChunkStoreFolderName
        fileName = chunk.fileName
        size = chunk.size
    }

    var chunk: RemoteFileChunk {
        RemoteFileChunk(fileName: fileName, size: size, remoteChunkStoreFolderName: remoteChunkStoreFolderName)
    }
}
