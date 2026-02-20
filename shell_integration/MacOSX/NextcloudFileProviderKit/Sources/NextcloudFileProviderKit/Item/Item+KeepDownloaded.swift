//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider

public extension Item {
    func toggle(keepDownloadedIn domain: NSFileProviderDomain) async throws {
        try await set(keepDownloaded: !keepDownloaded, domain: domain)
    }

    func set(keepDownloaded: Bool, domain: NSFileProviderDomain) async throws {
        _ = try dbManager.set(keepDownloaded: keepDownloaded, for: metadata)

        guard let manager = NSFileProviderManager(for: domain) else {
            if #available(iOS 17.1, macOS 14.1, *) {
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

        if keepDownloaded, !isDownloaded {
            try await manager.requestDownloadForItem(withIdentifier: itemIdentifier)
        } else {
            try await manager.requestModification(of: [.lastUsedDate], forItemWithIdentifier: itemIdentifier)
        }
    }
}
