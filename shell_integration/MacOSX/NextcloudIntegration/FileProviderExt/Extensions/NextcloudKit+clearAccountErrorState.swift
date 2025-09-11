//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import NextcloudKit
import os

extension NextcloudKit {
    ///
    /// Removes the given NextcloudKit accound identifier from the persisted list of accounts with certain errors managed by NextcloudKit.
    ///
    /// Copied from `removeServerErrorAccount(_:)` in the iOS Files app project as an interim solution.
    /// This implementation should actually be part of NextcloudKit and not the consuming projects.
    ///
    /// - Parameters:
    ///     - identifier: The composite account identifier string as typically used by NextcloudKit.
    ///
    static func clearAccountErrorState(for identifier: String) {
        let nkCommon = NextcloudKit.shared.nkCommonInstance
        let logger = Logger(subsystem: Bundle.main.bundleIdentifier!, category: "NextcloudKit+clearAccountErrorState")

        logger.debug("Attempt to clear account error state for account identifier: \"\(identifier)\"...")

        // MARK: User defaults group lookup.

        guard let groupDefaults = UserDefaults(suiteName: nkCommon.groupIdentifier) else {
            logger.error("Failed to get user defaults for NextcloudKit group identifier: \(nkCommon.groupIdentifier ?? "nil")")
            return
        }

        // MARK: Unauthorized state accounts.

        var unauthorized = groupDefaults.array(forKey: nkCommon.groupDefaultsUnauthorized) as? [String] ?? []
        logger.debug("Found list of accounts in unauthorized state:\n\(unauthorized.map { "\t- \($0)" }.joined(separator: "\n"))")

        unauthorized.removeAll { item in
            item == identifier
        }

        groupDefaults.set(unauthorized, forKey: nkCommon.groupDefaultsUnauthorized)

        // MARK: Unavailable state accounts.

        var unavailable = groupDefaults.array(forKey: nkCommon.groupDefaultsUnavailable) as? [String] ?? []
        logger.debug("Found list of accounts in unavailable state:\n\(unavailable.map { "\t- \($0)" }.joined(separator: "\n"))")

        unavailable.removeAll { item in
            item == identifier
        }

        groupDefaults.set(unavailable, forKey: nkCommon.groupDefaultsUnavailable)

        // MARK: Missing terms of service state accounts.

        var termsOfService = groupDefaults.array(forKey: nkCommon.groupDefaultsToS) as? [String] ?? []
        logger.debug("Found list of accounts in terms of service state:\n\(termsOfService.map { "\t- \($0)" }.joined(separator: "\n"))")

        termsOfService.removeAll { item in
            item == identifier
        }

        groupDefaults.set(termsOfService, forKey: nkCommon.groupDefaultsToS)

        // MARK: Forced persistence.

        groupDefaults.synchronize()
        logger.debug("Completed clearance of account error state call.")
    }
}
