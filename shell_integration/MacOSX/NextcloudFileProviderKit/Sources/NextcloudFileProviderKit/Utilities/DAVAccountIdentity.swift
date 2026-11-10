//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

/// The login, server URL and backend user ID used for Finder setup retries.
/// When converting absolute WebDAV parent URLs to paths such as `files:/Team`, the database
/// needs these fields to match the previous account's rows even after its Keychain password
/// is deleted or replaced.
struct DAVAccountIdentity: Equatable, Sendable {
    let username: String
    let serverBaseURL: String
    let backendUserID: String

    /// NextcloudKit's key combines the login and server URL. The WebDAV files URL uses the backend user ID.
    var accountKey: String {
        Account.ncKitAccountString(from: username, serverUrl: serverBaseURL)
    }

    var serializedFields: [String] { [accountKey, serverBaseURL, backendUserID] }

    init(username: String, serverBaseURL: String, backendUserID: String) {
        self.username = username
        self.serverBaseURL = serverBaseURL
        self.backendUserID = backendUserID
    }

    init?(serializedFields: [String]) {
        guard serializedFields.count == 3 else { return nil }
        let accountKeySuffix = " " + serializedFields[1]
        guard serializedFields[0].hasSuffix(accountKeySuffix) else { return nil }
        self.init(
            username: String(serializedFields[0].dropLast(accountKeySuffix.count)),
            serverBaseURL: serializedFields[1],
            backendUserID: serializedFields[2]
        )
    }

    /// Compare the exact bytes that SQLite uses to match account fields.
    func matchesByteForByte(_ other: Self) -> Bool {
        username.utf8.elementsEqual(other.username.utf8)
            && serverBaseURL.utf8.elementsEqual(other.serverBaseURL.utf8)
            && backendUserID.utf8.elementsEqual(other.backendUserID.utf8)
    }
}
