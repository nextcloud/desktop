/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import Foundation
import FileProvider

class FileProviderDomainNextcloudAccountData: NSObject {
    let webDavUrlSuffix: String = "/remote.php/dav"

    private(set) var username: String?
    private(set) var password: String?
    private(set) var serverUrl: URL?
    private(set) var davUrl: URL?

    var isNull: Bool {
        return username?.isEmpty ?? false || serverUrl?.absoluteString.isEmpty ?? false
    }

    override init() {
        super.init()
    }

    init(withKeychainAccount account:String) {
        super.init()
        getAccountDetails(keychainAccount: account)
    }

    static func getUserPasswordFromKeychain(accountString:String) -> Data? {
        let query = [
            kSecClass as String       : kSecClassGenericPassword,
            kSecAttrAccount as String : accountString,
            kSecReturnData as String  : kCFBooleanTrue!,
            kSecMatchLimit as String  : kSecMatchLimitOne
        ] as [String : Any]

        var dataTypeRef: AnyObject? = nil

        let status: OSStatus = SecItemCopyMatching(query as CFDictionary, &dataTypeRef)

        if status == noErr {
            return dataTypeRef as! Data?
        } else {
            return nil
        }
    }

    private func getAccountDetails(keychainAccount: String) {
        // The client sets the account field in the keychain entry as a colon-separated string consisting of
        // an account's username, its homeserver url, and the id of the account
        guard let passwordData = FileProviderDomainNextcloudAccountData.getUserPasswordFromKeychain(accountString: keychainAccount),
              let passwordString = String(data: passwordData, encoding: .utf8)
        else { return }

        let keychainAccountSplit = keychainAccount.split(separator: ":")
        let usernameSubstring = keychainAccountSplit[0]
        let serverUrlSubstring = keychainAccountSplit[1]
        let clientAccountIdSubstring = keychainAccountSplit[2]

        let usernameString = String(usernameSubstring)
        let serverUrlString = String(serverUrlSubstring)
        let clientAccountIdString = String(clientAccountIdSubstring)

        guard let serverUrlUrl = URL(string: String(serverUrlString)) else { return }
        let davUrlUrl = serverUrlUrl.appendingPathComponent(webDavUrlSuffix)

        username = usernameString
        password = passwordString
        serverUrl = serverUrlUrl
        davUrl = davUrlUrl
    }
}

