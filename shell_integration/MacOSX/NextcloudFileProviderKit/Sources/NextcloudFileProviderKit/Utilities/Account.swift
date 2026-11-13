//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudKit

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

    let davAccountIdentity: DAVAccountIdentity
    public let password: String
    public let ncKitAccount: String
    public let davFilesUrl: String
    public let trashUrl: String
    public let trashRestoreUrl: String
    public let fileName: String

    public var username: String { davAccountIdentity.username }
    public var id: String { davAccountIdentity.backendUserID }
    public var serverUrl: String { davAccountIdentity.serverBaseURL }

    public var description: String {
        ncKitAccount // Custom textual representation to avoid password leakage.
    }

    public static func ncKitAccountString(from username: String, serverUrl: String) -> String {
        username + " " + serverUrl
    }

    public init(user: String, id: String, serverUrl: String, password: String) {
        self.init(
            davAccountIdentity: DAVAccountIdentity(username: user, serverBaseURL: serverUrl, backendUserID: id),
            password: password
        )
    }

    init(davAccountIdentity: DAVAccountIdentity, password: String) {
        self.davAccountIdentity = davAccountIdentity
        self.password = password
        ncKitAccount = davAccountIdentity.accountKey
        let serverUrl = davAccountIdentity.serverBaseURL
        let id = davAccountIdentity.backendUserID
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
        guard let davPath = davPath(forURL: remotePath),
              davPath.root == .files,
              !davPath.path.isEmpty
        else { return nil }
        return String(davPath.path.dropFirst())
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
              let davPath = davPath(forURL: url)
        else {
            return false
        }
        return !davPath.path.split(separator: "/").contains("..")
    }

    /// The SDK either supplies the full root URL with a placeholder name or splits off the backend user ID.
    static func isFilesRootAddress(
        parentURL: String,
        fileName: String,
        accountBaseURL: String,
        backendUserID: String,
        acceptEquivalentResponseWebroot: Bool = false
    ) -> Bool {
        let rootURL: String
        switch fileName {
            case SendableItemMetadata.filesRootPlaceholderName, NextcloudKit.shared.nkCommonInstance.rootFileName:
                rootURL = parentURL
            default:
                guard !backendUserID.isEmpty, fileName.utf8.elementsEqual(backendUserID.utf8) else { return false }
                rootURL = parentURL + "/" + fileName
        }
        let path = davPath(
            forURL: rootURL,
            accountBaseURL: accountBaseURL,
            backendUserID: backendUserID,
            acceptEquivalentResponseWebroot: acceptEquivalentResponseWebroot
        )
        return path?.isFilesRoot == true
    }

    /// Normalize a recorded DAV parent for SQLite, including either Files root response format.
    static func normalizedDAVParent(
        parentURL: String,
        fileName: String,
        accountBaseURL: String,
        backendUserID: String
    ) -> (parent: DAVPath, isFilesRoot: Bool)? {
        if isFilesRootAddress(
            parentURL: parentURL,
            fileName: fileName,
            accountBaseURL: accountBaseURL,
            backendUserID: backendUserID,
            acceptEquivalentResponseWebroot: true
        ) {
            return (DAVPath(root: .files, path: ""), true)
        }

        guard let path = davPath(
            forURL: parentURL,
            accountBaseURL: accountBaseURL,
            backendUserID: backendUserID,
            acceptEquivalentResponseWebroot: true
        ) else { return nil }
        return (path, false)
    }

    /// Find the account's Files or Trash root in a URL and return the path below it.
    func davPath(forURL url: String) -> DAVPath? {
        Self.davPath(forURL: url, accountBaseURL: serverUrl, backendUserID: id)
    }

    /// Check whether a request URL and a DAV response URL name the same item under this
    /// account's Files or Trash root. NextcloudKit can return the server's spelling of the
    /// installation path, so compare the paths after removing the recognized DAV roots.
    func hasSameDAVPath(_ lhs: String, _ rhs: String) -> Bool {
        guard let lhsPath = davPath(forURL: lhs),
              let rhsPath = davPath(forURL: rhs)
        else { return false }
        return lhsPath == rhsPath
    }

    /// Find the account's WebDAV root in the URL and separate it from the rest of the path.
    /// For example, `https://cloud.example.com/nextcloud/remote.php/dav/files/alice/Team/Docs`
    /// has root `.files` and path `/Team/Docs`.
    /// Returns `nil` if neither the configured roots nor the forms returned by the DAV response parser match.
    ///
    /// Set `acceptEquivalentResponseWebroot` for URLs from DAV responses, including cached responses.
    /// A response can use different Unicode bytes for the installation path before `/remote.php`,
    /// such as `/caf\u{00E9}` and `/cafe\u{0301}`. The scheme, host, port and backend user ID
    /// still have to match byte for byte.
    static func davPath(
        forURL url: String,
        accountBaseURL: String,
        backendUserID: String,
        acceptEquivalentResponseWebroot: Bool = false
    ) -> DAVPath? {
        guard !backendUserID.isEmpty else { return nil }

        func pathRelativeToDAVRoot(underAccountBaseURL base: String) -> DAVPath? {
            for root in DAVRoot.allCases {
                if let suffix = pathSuffix(of: url, underRoot: root.rootURL(accountBaseURL: base, backendUserID: backendUserID)) {
                    return DAVPath(root: root, path: suffix)
                }
            }
            return nil
        }

        if let davPath = pathRelativeToDAVRoot(underAccountBaseURL: accountBaseURL) {
            return davPath
        }

        let decodedAccountBaseURL = percentDecodedAccountBaseURL(accountBaseURL)
        if decodedAccountBaseURL != accountBaseURL, let davPath = pathRelativeToDAVRoot(underAccountBaseURL: decodedAccountBaseURL) {
            return davPath
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

        func pathRelativeToDAVRoot(decodingInstallationPath pathComponents: [Substring]) -> DAVPath? {
            guard let decoded = pathComponents.joined(separator: "/").removingPercentEncoding else { return nil }
            guard !decoded.isEmpty else { return pathRelativeToDAVRoot(underAccountBaseURL: origin) }

            let webroot = "/" + decoded
            if let davPath = pathRelativeToDAVRoot(underAccountBaseURL: origin + webroot) {
                return davPath
            }

            guard acceptEquivalentResponseWebroot,
                  let responsePath = pathSuffix(of: url, underRoot: origin),
                  let responseWebroot = responsePath.range(of: webroot, options: .anchored)
            else {
                return nil
            }
            return pathRelativeToDAVRoot(underAccountBaseURL: origin + responsePath[responseWebroot])
        }

        if let davPath = pathRelativeToDAVRoot(decodingInstallationPath: installationPathComponents) {
            return davPath
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
        return pathRelativeToDAVRoot(decodingInstallationPath: dotResolvedPathComponents)
    }

    /// Build a request URL by appending the path to the account's Files or Trash root.
    static func absoluteURL(for davPath: DAVPath, accountBaseURL: String, backendUserID: String) -> String {
        davPath.root.rootURL(
            accountBaseURL: percentDecodedAccountBaseURL(accountBaseURL),
            backendUserID: backendUserID
        ) + davPath.path
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

        davAccountIdentity = DAVAccountIdentity(username: username, serverBaseURL: serverUrl, backendUserID: id)
        self.password = password
        self.ncKitAccount = ncKitAccount
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
