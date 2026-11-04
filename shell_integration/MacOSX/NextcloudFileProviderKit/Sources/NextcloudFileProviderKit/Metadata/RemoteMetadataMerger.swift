//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

/// Preserves local fields from an already-resolved item, including visit state only for directories.
enum RemoteMetadataMerger {
    static func merge(
        _ metadata: SendableItemMetadata,
        preservingLocalStateFrom existing: any ItemMetadata,
        preserveVisitedDirectory: Bool = true,
        preserveDownloadState: Bool = true,
        clearLockToken: Bool = false
    ) -> SendableItemMetadata {
        var merged = metadata
        if preserveDownloadState {
            merged.downloaded = existing.downloaded
        }
        merged.keepDownloaded = existing.keepDownloaded

        if metadata.directory, preserveVisitedDirectory {
            merged.visitedDirectory = existing.visitedDirectory
        }

        merged.lockToken = clearLockToken ? nil : existing.lockToken
        if merged.etag == existing.etag {
            merged.fileProviderContentVersion = existing.fileProviderContentVersion
        }

        return merged
    }
}
