//
//  Item+KeepOffline.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 13/5/25.
//

import FileProvider

public extension Item {
    func toggle(keepOfflineIn domain: NSFileProviderDomain) async throws {
        try await set(keepOffline: !keepOffline, domain: domain)
    }

    func set(keepOffline: Bool, domain: NSFileProviderDomain) async throws {
        var dirtyMetadata = metadata
        dirtyMetadata.keepOffline = keepOffline
        if #unavailable(macOS 13.0, iOS 16.0) {
            // We don't have a targeted method to force an update on this item on older OSes
            // So we will have to signal the enumerator in general to force an item on this update
            // (and assign a bad etag so we enumerate an update)
            dirtyMetadata.etag = "dirty"
        }
        dbManager.addItemMetadata(dirtyMetadata)

        guard let manager = NSFileProviderManager(for: domain) else {
            if #available(macOS 14.1, *) {
                throw NSFileProviderError(.providerDomainNotFound)
            } else {
                let providerDomainNotFoundErrorCode = -2013
                throw NSError(
                    domain: NSFileProviderErrorDomain,
                    code: providerDomainNotFoundErrorCode,
                    userInfo: [NSLocalizedDescriptionKey: "Failed to get manager for domain."]
                )
            }
        }

        if #available(macOS 13.0, iOS 16.0, visionOS 1.0, *) {
            if keepOffline && !isDownloaded {
                try await manager.requestDownloadForItem(withIdentifier: itemIdentifier)
            } else if !keepOffline && isDownloaded {
                try await manager.evictItem(identifier: itemIdentifier)
            } else {
                try await manager.requestModification(
                    of: [.lastUsedDate], forItemWithIdentifier: itemIdentifier
                )
            }
        } else {
            try await manager.signalEnumerator(for: .workingSet)
        }
    }
}
