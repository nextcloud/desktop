// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudFileProviderKit

public final class UploadChunkRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private var recordedChunks: [(fileName: String, size: Int64)] = []
    private var recordedChunksExisted = false

    public init() {}

    public var chunks: [(fileName: String, size: Int64)] {
        lock.lock()
        defer { lock.unlock() }
        return recordedChunks
    }

    public var chunksExisted: Bool {
        lock.lock()
        defer { lock.unlock() }
        return recordedChunksExisted
    }

    public func record(_ chunk: RemoteFileChunk, fileExists: Bool = false) {
        lock.lock()
        defer { lock.unlock() }
        recordedChunks.append((chunk.fileName, chunk.size))
        recordedChunksExisted = fileExists
    }
}
