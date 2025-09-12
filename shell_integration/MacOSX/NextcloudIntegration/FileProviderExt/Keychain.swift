//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import os

///
/// macOS keychain abstraction to fetch account passwords.
///
struct Keychain {
    ///
    /// Lookup a generic password for the given account on the given server.
    ///
    /// - Returns: `nil` in case of any error or the password not being found.
    ///
    static func getPassword(for account: String, on server: String) -> String? {
        Logger.keychain.debug("Looking for password of \"\(account)\" on \"\(server)\" in keychain...")

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
            Logger.keychain.error("Item not found!")
            return nil
        }

        guard status == errSecSuccess else {
            Logger.keychain.error("Keychain status: \(status)")
            return nil
        }

        guard let existingItem = item as? [String : Any], let passwordData = existingItem[kSecValueData as String] as? Data, let password = String(data: passwordData, encoding: String.Encoding.utf8) else {
            Logger.keychain.error("Unexpected password data!")
            return nil
        }

        Logger.keychain.debug("Found \(password.isEmpty ? "empty" : "non-empty") password for \"\(account)\" on \"\(server)\" in keychain.")

        return password
    }

    static func savePassword(_ password: String, for account: String, on server: String) {
        guard password.isEmpty == false else {
            Logger.keychain.error("Not saving password password for \"\(account)\" on \"\(server)\" because it is empty!")
            return
        }

        Logger.keychain.debug("Saving password for \"\(account)\" on \"\(server)\"...")

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
                Logger.keychain.debug("Succeeded to update password for \"\(account)\" on \"\(server)\" in keychain.")
            } else {
                Logger.keychain.error("Failed to update password for \"\(account)\" on \"\(server)\" in keychain. Status: \(updateStatus)")
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
                Logger.keychain.debug("Succeeded to add password for \"\(account)\" on \"\(server)\" in keychain.")
            } else {
                Logger.keychain.error("Failed to add password for \"\(account)\" on \"\(server)\" in keychain. Status: \(addStatus)")
            }
        } else {
            Logger.keychain.error("Failed to check for existing password for \"\(account)\" on \"\(server)\" in keychain. Status: \(status)")
        }
    }
}
