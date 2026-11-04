//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

extension ItemMetadata {
    /// Merges incoming remote metadata with this item's local state.
    ///
    /// - `keepDownloaded` is always preserved.
    /// - `downloaded` is preserved when `preserveDownloadState` is enabled.
    /// - `visitedDirectory` is preserved for directories when `preserveVisitedDirectory` is enabled.
    /// - `lockToken` is preserved, taken from the incoming metadata, or cleared according to `lock`.
    /// - `fileProviderContentVersion` is preserved when the incoming and stored ETags match.
    func mergingRemoteMetadata(
        _ metadata: SendableItemMetadata,
        preserveVisitedDirectory: Bool,
        preserveDownloadState: Bool,
        lock: LockTokenMergePolicy
    ) -> SendableItemMetadata {
        var merged = metadata
        if preserveDownloadState {
            merged.downloaded = downloaded
        }
        merged.keepDownloaded = keepDownloaded

        if metadata.directory, preserveVisitedDirectory {
            merged.visitedDirectory = visitedDirectory
        }

        merged.lockToken = switch lock {
            case .preserveLocal: lockToken
            case .useIncoming: metadata.lockToken
            case .clear: nil
        }
        if merged.etag == etag {
            merged.fileProviderContentVersion = fileProviderContentVersion
        }

        return merged
    }
}
