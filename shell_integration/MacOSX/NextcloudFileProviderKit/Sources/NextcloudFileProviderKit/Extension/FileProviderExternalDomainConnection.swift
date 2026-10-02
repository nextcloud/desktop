//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

/// Decides whether an external-volume domain belongs to an account configured for external storage on this Mac.
enum FileProviderExternalDomainConnection {
    static let accountIdentifierUserInfoKey = "org.nextcloud.desktop.accountIdentifier"
    static let accountIdentifiersDefaultsKey = "NCExternalVolumeAccountIdentifiers"

    static func locallyConfiguredAccountIdentifiers(bundle: Bundle = .main) -> Set<String>? {
        guard
            let extensionDictionary = bundle.infoDictionary?["NSExtension"] as? [String: Any],
            let appGroupIdentifier = extensionDictionary["NSExtensionFileProviderDocumentGroup"] as? String,
            let defaults = UserDefaults(suiteName: appGroupIdentifier)
        else {
            return nil
        }

        return Set(defaults.stringArray(forKey: accountIdentifiersDefaultsKey) ?? [])
    }

    static func shouldConnect(
        domainUserInfo: [AnyHashable: Any]?,
        locallyConfiguredAccountIdentifiers: Set<String>
    ) -> Bool {
        guard let accountIdentifier = domainUserInfo?[accountIdentifierUserInfoKey] as? String else {
            return false
        }

        return locallyConfiguredAccountIdentifiers.contains(accountIdentifier)
    }
}
