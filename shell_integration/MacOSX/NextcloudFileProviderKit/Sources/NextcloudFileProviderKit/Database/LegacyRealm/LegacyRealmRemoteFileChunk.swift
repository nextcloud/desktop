//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation // `@objc(...)` below needs it in scope.
import RealmSwift

///
/// Realm row of one recorded upload chunk.
///
/// The Objective-C name keeps the on-disk object name `RemoteFileChunk`, which the Swift name gave up to the value type ``RemoteFileChunk``.
///
@objc(RemoteFileChunk)
final class LegacyRealmRemoteFileChunk: Object {
    @Persisted var fileName: String
    @Persisted var size: Int64
    @Persisted var remoteChunkStoreFolderName: String

    convenience init(_ chunk: RemoteFileChunk) {
        self.init()
        fileName = chunk.fileName
        size = chunk.size
        remoteChunkStoreFolderName = chunk.remoteChunkStoreFolderName
    }

    var chunk: RemoteFileChunk {
        RemoteFileChunk(fileName: fileName, size: size, remoteChunkStoreFolderName: remoteChunkStoreFolderName)
    }
}
