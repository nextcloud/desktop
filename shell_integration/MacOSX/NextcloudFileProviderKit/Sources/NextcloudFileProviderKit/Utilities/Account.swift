//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

let AccountDictUsernameKey = "usernameKey"
let AccountDictIdKey = "idKey"
let AccountDictPasswordKey = "passwordKey"
let AccountDictNcKitAccountKey = "ncKitAccountKey"
let AccountDictServerUrlKey = "serverUrlKey"
let AccountDictDavFilesUrlKey = "davFilesUrlKey"
let AccountDictTrashUrlKey = "trashUrlKey"
let AccountDictTrashRestoreUrlKey = "trashRestoreUrlKey"
let AccountDictFileNameKey = "fileNameKey"

///
/// Ephemeral data model which provides account information associated with a file provider domain.
///
public struct Account: CustomStringConvertible, Equatable, Sendable {
    public static let webDavFilesUrlSuffix = "/remote.php/dav/files/"
    public static let webDavTrashUrlSuffix = "/remote.php/dav/trashbin/"

    public let username: String
    public let id: String
    public let password: String
    public let ncKitAccount: String
    public let serverUrl: String
    public let davFilesUrl: String
    public let trashUrl: String
    public let trashRestoreUrl: String
    public let fileName: String

    public var description: String {
        ncKitAccount // Custom textual representation to avoid password leakage.
    }

    public static func ncKitAccountString(from username: String, serverUrl: String) -> String {
        username + " " + serverUrl
    }

    public init(user: String, id: String, serverUrl: String, password: String) {
        username = user
        self.id = id
        self.password = password
        ncKitAccount = Self.ncKitAccountString(from: user, serverUrl: serverUrl)
        self.serverUrl = serverUrl
        davFilesUrl = serverUrl + Self.webDavFilesUrlSuffix + id
        trashUrl = serverUrl + Self.webDavTrashUrlSuffix + id + "/trash"
        trashRestoreUrl = serverUrl + Self.webDavTrashUrlSuffix + id + "/restore"

        let sanitisedUrl = (URL(string: serverUrl)?.safeFilenameFromURLString() ?? "unknown")
        fileName = sanitise(string: id) + "_" + sanitisedUrl
    }

    /// Returns the path below this account's WebDAV Files root, without the leading slash.
    /// Accepts the configured root and the root as returned by NextcloudKit's DAV parser.
    public func filesRootRelativePath(for remotePath: String) -> String? {
        func pathBelowFilesRoot(_ root: String) -> String? {
            let prefix = (root + "/").utf8
            guard remotePath.utf8.starts(with: prefix) else { return nil }
            return String(decoding: remotePath.utf8.dropFirst(prefix.count), as: UTF8.self)
        }

        if let path = pathBelowFilesRoot(davFilesUrl) {
            return path
        }

        guard davFilesUrl.utf8.starts(with: (serverUrl + "/").utf8),
              let configuredURL = URL(string: serverUrl),
              let scheme = configuredURL.scheme,
              let host = configuredURL.host
        else { return nil }

        let davPath = String(decoding: davFilesUrl.utf8.dropFirst(serverUrl.utf8.count), as: UTF8.self)
        let port = configuredURL.port.map { ":\($0)" } ?? ""
        let origin = "\(scheme)://\(host)\(port)"

        // NextcloudKit collapses separators before decoding the response path.
        // The server may also resolve dot segments in the installation path.
        for base in [configuredURL, configuredURL.standardized] {
            let encodedPath = base.path(percentEncoded: true).split(separator: "/").joined(separator: "/")
            guard let decodedPath = encodedPath.removingPercentEncoding else { continue }
            let responseBase = decodedPath.isEmpty ? origin : origin + "/" + decodedPath
            if let path = pathBelowFilesRoot(responseBase + davPath) {
                return path
            }
        }
        return nil
    }

    public init?(dictionary: [String: String]) {
        guard let username = dictionary[AccountDictUsernameKey],
              let id = dictionary[AccountDictIdKey],
              let password = dictionary[AccountDictPasswordKey],
              let ncKitAccount = dictionary[AccountDictNcKitAccountKey],
              let serverUrl = dictionary[AccountDictServerUrlKey],
              let davFilesUrl = dictionary[AccountDictDavFilesUrlKey],
              let trashUrl = dictionary[AccountDictTrashUrlKey],
              let trashRestoreUrl = dictionary[AccountDictTrashRestoreUrlKey],
              let fileName = dictionary[AccountDictFileNameKey]
        else {
            return nil
        }

        self.username = username
        self.id = id
        self.password = password
        self.ncKitAccount = ncKitAccount
        self.serverUrl = serverUrl
        self.davFilesUrl = davFilesUrl
        self.trashUrl = trashUrl
        self.trashRestoreUrl = trashRestoreUrl
        self.fileName = fileName
    }

    public func dictionary() -> [String: String] {
        [
            AccountDictUsernameKey: username,
            AccountDictIdKey: id,
            AccountDictPasswordKey: password,
            AccountDictNcKitAccountKey: ncKitAccount,
            AccountDictServerUrlKey: serverUrl,
            AccountDictDavFilesUrlKey: davFilesUrl,
            AccountDictTrashUrlKey: trashUrl,
            AccountDictTrashRestoreUrlKey: trashRestoreUrl,
            AccountDictFileNameKey: fileName
        ]
    }
}
