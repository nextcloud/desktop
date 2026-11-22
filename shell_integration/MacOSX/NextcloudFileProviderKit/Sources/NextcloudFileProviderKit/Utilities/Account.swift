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

    /// Returns a remote path relative to this account's DAV files root.
    ///
    /// Paths outside the files root, such as trash paths, do not have a sync-relative path.
    public func filesRootRelativePath(for remotePath: String) -> String? {
        let filesRootPrefix = davFilesUrl + "/"
        guard remotePath.hasPrefix(filesRootPrefix) else { return nil }
        return String(remotePath.dropFirst(filesRootPrefix.count))
    }

    func ownsDAVReadURL(_ url: String, sourceMetadata: SendableItemMetadata? = nil) -> Bool {
        func matches(_ root: String) -> Bool {
            url.utf8.elementsEqual(root.utf8) || url.utf8.starts(with: (root + "/").utf8)
        }

        let roots = [davFilesUrl, trashUrl]
        if roots.contains(where: matches) {
            return true
        }

        // A normalized URL can coincide with a previous base. Use the row's account provenance to distinguish them.
        guard let sourceMetadata,
              sourceMetadata.urlBase.utf8.elementsEqual(serverUrl.utf8),
              sourceMetadata.userId.utf8.elementsEqual(id.utf8)
        else {
            return false
        }

        // NextcloudKit joins the scheme, host and port with the decoded DAV parent path.
        guard let configuredURL = URL(string: serverUrl),
              let scheme = configuredURL.scheme,
              let host = configuredURL.host
        else {
            return false
        }

        // Like NSString.deletingLastPathComponent in the DAV parser, collapse separators before decoding.
        let pathComponents = configuredURL.path(percentEncoded: true).split(separator: "/")
        let port = configuredURL.port.map { ":\($0)" } ?? ""
        let origin = "\(scheme)://\(host)\(port)"
        let configuredPrefix = (serverUrl + "/").utf8

        func matchesResponseRoot(webrootComponents: [Substring]) -> Bool {
            guard let path = webrootComponents.joined(separator: "/").removingPercentEncoding else {
                return false
            }
            let responseBase = path.isEmpty ? origin : origin + "/" + path
            return roots.contains { root in
                guard root.utf8.starts(with: configuredPrefix) else { return false }
                let davPath = String(decoding: root.utf8.dropFirst(serverUrl.utf8.count), as: UTF8.self)
                return matches(responseBase + davPath)
            }
        }

        if matchesResponseRoot(webrootComponents: pathComponents) {
            return true
        }

        // A reverse proxy can resolve dot segments in the webroot before forwarding the request.
        let canonicalComponents = pathComponents.reduce(into: [Substring]()) { components, component in
            switch component {
                case ".":
                    break
                case "..":
                    _ = components.popLast()
                default:
                    components.append(component)
            }
        }
        return canonicalComponents != pathComponents && matchesResponseRoot(webrootComponents: canonicalComponents)
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
