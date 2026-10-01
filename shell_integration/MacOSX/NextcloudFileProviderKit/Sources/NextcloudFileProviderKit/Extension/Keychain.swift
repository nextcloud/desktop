//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import Security

///
/// Stores the password of a file provider domain as a generic password keyed by the domain identifier and the user name.
///
/// Generic passwords ignore internet password attributes like `kSecAttrServer`, so never use them to scope these items.
///
struct Keychain {
    let logger: FileProviderLogger

    ///
    /// The domain identifier, used as the service of the items.
    ///
    let service: String

    ///
    /// The domain display name, shown as the item name in Keychain Access.
    ///
    let label: String

    init(domain: NSFileProviderDomain, log: any FileProviderLogging) {
        logger = FileProviderLogger(category: "Keychain", log: log)
        service = domain.identifier.rawValue
        label = domain.displayName
    }

    // MARK: - Queries

    ///
    /// Matches the one item of the given account in the given domain.
    ///
    /// - Returns: `nil` if the service or the account is empty, because the query would then match items of other applications.
    ///
    static func itemQuery(service: String, account: String) -> [String: Any]? {
        guard service.isEmpty == false, account.isEmpty == false else {
            return nil
        }

        return [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account
        ]
    }

    static func lookupQuery(service: String, account: String) -> [String: Any]? {
        guard var query = itemQuery(service: service, account: account) else {
            return nil
        }

        query[kSecMatchLimit as String] = kSecMatchLimitOne
        query[kSecReturnData as String] = true

        return query
    }

    static func addAttributes(service: String, account: String, label: String, password: Data) -> [String: Any]? {
        guard var attributes = itemQuery(service: service, account: account) else {
            return nil
        }

        attributes[kSecAttrLabel as String] = label
        attributes[kSecValueData as String] = password

        return attributes
    }

    static func updateAttributes(label: String, password: Data) -> [String: Any] {
        [
            kSecAttrLabel as String: label,
            kSecValueData as String: password
        ]
    }

    static func statusError(_ status: OSStatus) -> NSError {
        NSError(domain: NSOSStatusErrorDomain, code: Int(status))
    }

    // MARK: - Operations

    ///
    /// - Returns: `nil` in case of any error or the password not being found.
    ///
    func getPassword(for account: String) -> String? {
        guard let query = Self.lookupQuery(service: service, account: account) else {
            logger.error("Cannot look up a password without domain identifier and account.", [.account: account, .domain: service])
            return nil
        }

        var item: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &item)

        guard status != errSecItemNotFound else {
            logger.info("No password found in keychain.", [.account: account, .domain: service])
            return nil
        }

        guard status == errSecSuccess else {
            logger.error("Failed to look up password in keychain.", [.account: account, .domain: service, .error: Self.statusError(status)])
            return nil
        }

        guard let data = item as? Data, let password = String(data: data, encoding: .utf8) else {
            logger.error("Unexpected password data in keychain.", [.account: account, .domain: service])
            return nil
        }

        logger.debug("Found password in keychain.", [.account: account, .domain: service])

        return password
    }

    ///
    /// - Returns: `true` if the password is stored afterwards.
    ///
    @discardableResult
    func savePassword(_ password: String, for account: String) -> Bool {
        guard password.isEmpty == false else {
            logger.error("Not saving an empty password.", [.account: account, .domain: service])
            return false
        }

        let data = Data(password.utf8)

        guard let query = Self.itemQuery(service: service, account: account), let attributes = Self.addAttributes(service: service, account: account, label: label, password: data) else {
            logger.error("Cannot save a password without domain identifier and account.", [.account: account, .domain: service])
            return false
        }

        let addStatus = SecItemAdd(attributes as CFDictionary, nil)

        if addStatus == errSecSuccess {
            logger.debug("Added password to keychain.", [.account: account, .domain: service])
            return true
        }

        guard addStatus == errSecDuplicateItem else {
            logger.error("Failed to add password to keychain.", [.account: account, .domain: service, .error: Self.statusError(addStatus)])
            return false
        }

        let updateStatus = SecItemUpdate(query as CFDictionary, Self.updateAttributes(label: label, password: data) as CFDictionary)

        guard updateStatus == errSecSuccess else {
            logger.error("Failed to update password in keychain.", [.account: account, .domain: service, .error: Self.statusError(updateStatus)])
            return false
        }

        logger.debug("Updated password in keychain.", [.account: account, .domain: service])

        return true
    }
}
