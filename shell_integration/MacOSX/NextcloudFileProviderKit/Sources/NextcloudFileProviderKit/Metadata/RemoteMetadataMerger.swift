//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

/// Preserves local fields from an already-resolved item when replacing its server metadata.
enum RemoteMetadataMerger {
    static func merge(
        _ metadata: SendableItemMetadata,
        preservingLocalStateFrom existing: any ItemMetadata,
        preserveVisitedDirectory: Bool
    ) -> SendableItemMetadata {
        var merged = metadata
        merged.downloaded = existing.downloaded
        merged.keepDownloaded = existing.keepDownloaded

        if preserveVisitedDirectory {
            merged.visitedDirectory = existing.visitedDirectory
        }

        merged.lockToken = existing.lockToken
        if merged.etag == existing.etag {
            merged.fileProviderContentVersion = existing.fileProviderContentVersion
        }

        return merged
    }
}
