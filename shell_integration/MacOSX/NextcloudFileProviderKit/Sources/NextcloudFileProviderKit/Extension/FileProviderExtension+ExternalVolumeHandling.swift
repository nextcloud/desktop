//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation

@available(macOS 15.0, *)
extension FileProviderExtension: NSFileProviderExternalVolumeHandling {
    public func shouldConnectExternalDomain(completionHandler: @escaping (Error?) -> Void) {
        guard
            let locallyConfiguredAccountIdentifiers = FileProviderExternalDomainConnection.locallyConfiguredAccountIdentifiers(),
            FileProviderExternalDomainConnection.shouldConnect(
                domainUserInfo: domain.userInfo,
                locallyConfiguredAccountIdentifiers: locallyConfiguredAccountIdentifiers
            )
        else {
            logger.info(
                "Leaving external File Provider domain disconnected because its account is not configured for external storage on this Mac.",
                [.domain: domain.identifier.rawValue]
            )
            completionHandler(NSFileProviderError(.notAuthenticated))
            return
        }

        completionHandler(nil)
    }
}
