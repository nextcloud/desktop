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

let AccountDictUsernameKey = "usernameKey"
let AccountDictIdKey = "idKey"
let AccountDictPasswordKey = "passwordKey"
let AccountDictNcKitAccountKey = "ncKitAccountKey"
let AccountDictServerUrlKey = "serverUrlKey"
let AccountDictDavFilesUrlKey = "davFilesUrlKey"

public struct Account: Equatable {
    public static let webDavFilesUrlSuffix: String = "/remote.php/dav/files/"
    public let username, id, password, ncKitAccount, serverUrl, davFilesUrl: String

    public init(user: String, id: String, serverUrl: String, password: String) {
        username = user
        self.id = id
        self.password = password
        ncKitAccount = user + " " + serverUrl
        self.serverUrl = serverUrl
        davFilesUrl = serverUrl + Self.webDavFilesUrlSuffix + user
    }

    public init?(dictionary: Dictionary<String, String>) {
        guard let username = dictionary[AccountDictUsernameKey],
              let id = dictionary[AccountDictIdKey],
              let password = dictionary[AccountDictPasswordKey],
              let ncKitAccount = dictionary[AccountDictNcKitAccountKey],
              let serverUrl = dictionary[AccountDictServerUrlKey],
              let davFilesUrl = dictionary[AccountDictDavFilesUrlKey]
        else {
            return nil
        }

        self.username = username
        self.id = id
        self.password = password
        self.ncKitAccount = ncKitAccount
        self.serverUrl = serverUrl
        self.davFilesUrl = davFilesUrl
    }

    public func dictionary() -> Dictionary<String, String> {
        return [
            AccountDictUsernameKey: username,
            AccountDictPasswordKey: password,
            AccountDictNcKitAccountKey: ncKitAccount,
            AccountDictServerUrlKey: serverUrl,
            AccountDictDavFilesUrlKey: davFilesUrl
        ]
    }
}

