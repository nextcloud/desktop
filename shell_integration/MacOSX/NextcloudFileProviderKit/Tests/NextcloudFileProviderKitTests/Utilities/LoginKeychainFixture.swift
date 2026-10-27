//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import Security

///
/// Direct access to generic passwords in the default keychain, independent of the code under test.
///
/// Every helper requires a non-empty service or account, because an empty one would match items of other applications.
///
enum LoginKeychainFixture {
    ///
    /// Label of every item planted by tests, so leftovers can be found in Keychain Access.
    ///
    static let label = "NextcloudFileProviderKitTests"

    ///
    /// Set to `1` to fail instead of skipping when the default keychain is not usable, for example in CI.
    ///
    static let requiredEnvironmentVariable = "NEXTCLOUD_REQUIRE_LOGIN_KEYCHAIN_TESTS"

    private static let maximumDeletionCount = 32

    static var isRequired: Bool {
        ProcessInfo.processInfo.environment[requiredEnvironmentVariable] == "1"
    }

    ///
    /// Whether items can be added and removed without the system asking the user to unlock the keychain.
    ///
    static func isUsable() -> Bool {
        var keychain: SecKeychain?

        guard SecKeychainCopyDefault(&keychain) == errSecSuccess, let keychain else {
            return false
        }

        var status = SecKeychainStatus()

        guard SecKeychainGetStatus(keychain, &status) == errSecSuccess, status & SecKeychainStatus(kSecUnlockStateStatus) != 0 else {
            return false
        }

        let probe = "nextcloud-test-probe-\(UUID().uuidString)"

        guard plant(service: probe, account: probe, password: "probe") == errSecSuccess else {
            return false
        }

        removeAll(service: probe)

        return count(service: probe) == 0
    }

    @discardableResult
    static func plant(service: String, account: String, password: String, label: String = label) -> OSStatus {
        precondition(service.isEmpty == false && account.isEmpty == false)

        let attributes: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecAttrLabel as String: label,
            kSecValueData as String: Data(password.utf8)
        ]

        return SecItemAdd(attributes as CFDictionary, nil)
    }

    static func password(service: String, account: String) -> String? {
        precondition(service.isEmpty == false && account.isEmpty == false)

        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecMatchLimit as String: kSecMatchLimitOne,
            kSecReturnData as String: true
        ]

        var item: CFTypeRef?

        guard SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess, let data = item as? Data else {
            return nil
        }

        return String(data: data, encoding: .utf8)
    }

    static func label(service: String, account: String) -> String? {
        precondition(service.isEmpty == false && account.isEmpty == false)

        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecMatchLimit as String: kSecMatchLimitOne,
            kSecReturnAttributes as String: true
        ]

        var item: CFTypeRef?

        guard SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess, let attributes = item as? [String: Any] else {
            return nil
        }

        return attributes[kSecAttrLabel as String] as? String
    }

    static func count(service: String) -> Int {
        precondition(service.isEmpty == false)

        return count(matching: [kSecAttrService as String: service])
    }

    static func count(account: String) -> Int {
        precondition(account.isEmpty == false)

        return count(matching: [kSecAttrAccount as String: account])
    }

    static func removeAll(service: String) {
        precondition(service.isEmpty == false)
        removeAll(matching: [kSecAttrService as String: service])
    }

    static func removeAll(account: String) {
        precondition(account.isEmpty == false)
        removeAll(matching: [kSecAttrAccount as String: account])
    }

    private static func count(matching attributes: [String: Any]) -> Int {
        var query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecMatchLimit as String: kSecMatchLimitAll,
            kSecReturnAttributes as String: true
        ]

        query.merge(attributes) { _, new in new }

        var items: CFTypeRef?

        guard SecItemCopyMatching(query as CFDictionary, &items) == errSecSuccess else {
            return 0
        }

        return (items as? [[String: Any]])?.count ?? 0
    }

    private static func removeAll(matching attributes: [String: Any]) {
        var query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword
        ]

        query.merge(attributes) { _, new in new }

        for _ in 0 ..< maximumDeletionCount {
            guard SecItemDelete(query as CFDictionary) == errSecSuccess else {
                return
            }
        }
    }
}
