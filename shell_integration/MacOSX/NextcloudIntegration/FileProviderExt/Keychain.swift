//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudFileProviderKit
import os

///
/// macOS keychain abstraction to fetch account passwords.
///
struct Keychain {
    let logger: FileProviderLogger

    init(log: any FileProviderLogging) {
        self.logger = FileProviderLogger(category: "Keychain", log: log)
    }

    ///
    /// Lookup a generic password for the given account on the given server.
    ///
    /// - Returns: `nil` in case of any error or the password not being found.
    ///
    func getPassword(for account: String, on server: String) -> String? {
        logger.debug("Looking for password of \"\(account)\" on \"\(server)\" in keychain...")

        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: account,
            kSecAttrServer as String: server,
            kSecMatchLimit as String: kSecMatchLimitOne,
            kSecReturnAttributes as String: true,
            kSecReturnData as String: true
        ]

        var item: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &item)

        guard status != errSecItemNotFound else {
            logger.error("Item not found!")
            return nil
        }

        guard status == errSecSuccess else {
            logger.error("Keychain status: \(status)")
            return nil
        }

        guard let existingItem = item as? [String : Any], let passwordData = existingItem[kSecValueData as String] as? Data, let password = String(data: passwordData, encoding: String.Encoding.utf8) else {
            logger.error("Unexpected password data!")
            return nil
        }

        logger.debug("Found \(password.isEmpty ? "empty" : "non-empty") password for \"\(account)\" on \"\(server)\" in keychain.")

        return password
    }

    func savePassword(_ password: String, for account: String, on server: String) {
        guard password.isEmpty == false else {
            logger.error("Not saving password password for \"\(account)\" on \"\(server)\" because it is empty!")
            return
        }

        logger.debug("Saving password for \"\(account)\" on \"\(server)\"...")

        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: account,
            kSecAttrServer as String: server
        ]

        // First, check if an item already exists
        let status = SecItemCopyMatching(query as CFDictionary, nil)

        if status == errSecSuccess {
            // Item exists, update it
            let updateAttributes: [String: Any] = [
                kSecValueData as String: password.data(using: .utf8)!
            ]

            let updateStatus = SecItemUpdate(query as CFDictionary, updateAttributes as CFDictionary)

            if updateStatus == errSecSuccess {
                logger.debug("Succeeded to update password for \"\(account)\" on \"\(server)\" in keychain.")
            } else {
                logger.error("Failed to update password for \"\(account)\" on \"\(server)\" in keychain. Status: \(updateStatus)")
            }
        } else if status == errSecItemNotFound {
            // Item doesn't exist, add a new one
            let addQuery: [String: Any] = [
                kSecClass as String: kSecClassGenericPassword,
                kSecAttrAccount as String: account,
                kSecAttrServer as String: server,
                kSecValueData as String: password.data(using: .utf8)!
            ]

            let addStatus = SecItemAdd(addQuery as CFDictionary, nil)

            if addStatus == errSecSuccess {
                logger.debug("Succeeded to add password for \"\(account)\" on \"\(server)\" in keychain.")
            } else {
                logger.error("Failed to add password for \"\(account)\" on \"\(server)\" in keychain. Status: \(addStatus)")
            }
        } else {
            logger.error("Failed to check for existing password for \"\(account)\" on \"\(server)\" in keychain. Status: \(status)")
        }
    }
}
