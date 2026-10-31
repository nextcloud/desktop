//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudKit

extension NextcloudKit {
    /// Installs account credentials and removes persisted errors that would block new requests.
    public func appendSession(account: Account, userAgent: String) {
        clearAccountErrorState(for: account.ncKitAccount)

        appendSession(
            account: account.ncKitAccount,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id,
            password: account.password,
            userAgent: userAgent,
            groupIdentifier: ""
        )
    }

    private func clearAccountErrorState(for identifier: String) {
        let nkCommon = nkCommonInstance
        guard let groupDefaults = UserDefaults(suiteName: nkCommon.groupIdentifier) else {
            return
        }

        for key in [nkCommon.groupDefaultsUnauthorized, nkCommon.groupDefaultsUnavailable, nkCommon.groupDefaultsToS] {
            let accounts = groupDefaults.array(forKey: key) as? [String] ?? []
            groupDefaults.set(accounts.filter { $0 != identifier }, forKey: key)
        }

        groupDefaults.synchronize()
    }
}
