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
    private enum DAVRoot: String, CaseIterable {
        case files = "files:"
        case trash = "trash:"

        func rootURL(accountBaseURL: String, backendUserID: String) -> String {
            switch self {
                case .files:
                    return accountBaseURL + Account.webDavFilesUrlSuffix + backendUserID
                case .trash:
                    return accountBaseURL + Account.webDavTrashUrlSuffix + backendUserID + "/trash"
            }
        }
    }

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
        let davAccountBaseURL = Self.percentDecodedAccountBaseURL(serverUrl)
        davFilesUrl = DAVRoot.files.rootURL(accountBaseURL: davAccountBaseURL, backendUserID: id)
        trashUrl = DAVRoot.trash.rootURL(accountBaseURL: davAccountBaseURL, backendUserID: id)
        trashRestoreUrl = davAccountBaseURL + Self.webDavTrashUrlSuffix + id + "/restore"

        let sanitisedUrl = (URL(string: serverUrl)?.safeFilenameFromURLString() ?? "unknown")
        fileName = sanitise(string: id) + "_" + sanitisedUrl
    }

    /// Returns a remote path relative to this account's DAV files root.
    ///
    /// Paths outside the files root, such as trash paths, do not have a sync-relative path.
    public func filesRootRelativePath(for remotePath: String) -> String? {
        let prefix = DAVRoot.files.rawValue + "/"
        guard let davPath = Self.davRelativePath(forURL: remotePath, accountBaseURL: serverUrl, backendUserID: id),
              davPath.hasPrefix(prefix)
        else { return nil }
        return String(davPath.dropFirst(prefix.count))
    }

    func ownsDAVReadURL(_ url: String, sourceMetadata: SendableItemMetadata? = nil) -> Bool {
        for root in [davFilesUrl, trashUrl] {
            if let suffix = Self.pathSuffix(of: url, underRoot: root) {
                return !suffix.split(separator: "/").contains("..")
            }
        }

        // A parser-normalized URL can also match an older account's configured root.
        guard let sourceMetadata,
              sourceMetadata.urlBase.utf8.elementsEqual(serverUrl.utf8),
              sourceMetadata.userId.utf8.elementsEqual(id.utf8),
              let davPath = Self.davRelativePath(forURL: url, accountBaseURL: serverUrl, backendUserID: id)
        else {
            return false
        }
        return !davPath.split(separator: "/").contains("..")
    }

    static func isStoredFilesRootLocation(_ location: String) -> Bool {
        location == DAVRoot.files.rawValue || location == DAVRoot.files.rawValue + "/"
    }

    static func isStoredTrashLocation(_ location: String) -> Bool {
        pathSuffix(of: location, underRoot: DAVRoot.trash.rawValue) != nil
    }

    /// Replace the full WebDAV files or trash root at the start of the URL with `files:` or `trash:`.
    /// For example, `https://cloud.example.com/nextcloud/remote.php/dav/files/alice/Team/Docs` becomes `files:/Team/Docs`.
    /// Returns `nil` if neither the configured roots nor the forms returned by the DAV response parser match.
    static func davRelativePath(forURL url: String, accountBaseURL: String, backendUserID: String) -> String? {
        guard !backendUserID.isEmpty else { return nil }

        func relativeLocation(underAccountBaseURL base: String) -> String? {
            for root in DAVRoot.allCases {
                if let suffix = pathSuffix(of: url, underRoot: root.rootURL(accountBaseURL: base, backendUserID: backendUserID)) {
                    return root.rawValue + suffix
                }
            }
            return nil
        }

        if let location = relativeLocation(underAccountBaseURL: accountBaseURL) {
            return location
        }

        let decodedAccountBaseURL = percentDecodedAccountBaseURL(accountBaseURL)
        if decodedAccountBaseURL != accountBaseURL, let location = relativeLocation(underAccountBaseURL: decodedAccountBaseURL) {
            return location
        }

        // NextcloudKit builds the response URL from the origin and the decoded DAV href.
        guard let configuredURL = URL(string: accountBaseURL),
              let scheme = configuredURL.scheme,
              let host = configuredURL.host
        else {
            return nil
        }

        let port = configuredURL.port.map { ":\($0)" } ?? ""
        let origin = "\(scheme)://\(host)\(port)"
        let installationPathComponents = configuredURL.path(percentEncoded: true).split(separator: "/")

        func parserBaseURL(forPathComponents pathComponents: [Substring]) -> String? {
            guard let decoded = pathComponents.joined(separator: "/").removingPercentEncoding else { return nil }
            return decoded.isEmpty ? origin : origin + "/" + decoded
        }

        if let base = parserBaseURL(forPathComponents: installationPathComponents), let location = relativeLocation(underAccountBaseURL: base) {
            return location
        }

        // A reverse proxy may also resolve dot segments in the configured webroot.
        let dotResolvedPathComponents = installationPathComponents.reduce(into: [Substring]()) { components, component in
            switch component {
                case ".":
                    break
                case "..":
                    _ = components.popLast()
                default:
                    components.append(component)
            }
        }
        guard dotResolvedPathComponents != installationPathComponents else { return nil }
        return parserBaseURL(forPathComponents: dotResolvedPathComponents).flatMap(relativeLocation(underAccountBaseURL:))
    }

    /// Expand a `files:` or `trash:` path into an absolute URL for the account.
    static func absoluteURL(fromDAVRelativePath path: String, accountBaseURL: String, backendUserID: String) -> String? {
        for root in DAVRoot.allCases {
            if let suffix = pathSuffix(of: path, underRoot: root.rawValue) {
                return root.rootURL(accountBaseURL: percentDecodedAccountBaseURL(accountBaseURL), backendUserID: backendUserID) + suffix
            }
        }
        return nil
    }

    /// NextcloudKit applies URL encoding when sending a request. Decode only the
    /// configured base, since item paths returned by the DAV parser are already decoded.
    private static func percentDecodedAccountBaseURL(_ base: String) -> String {
        guard base.contains("%"), base.contains("://") else { return base }
        return base.removingPercentEncoding ?? base
    }

    /// Compare DAV roots byte-for-byte, including the path segment boundary.
    private static func pathSuffix(of url: String, underRoot root: String) -> String? {
        if url.utf8.elementsEqual(root.utf8) {
            return ""
        }
        guard url.utf8.starts(with: (root + "/").utf8) else { return nil }
        return String(decoding: url.utf8.dropFirst(root.utf8.count), as: UTF8.self)
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
