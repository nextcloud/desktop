//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

///
/// A predefined set of detail keys to avoid having multiple keys for the same type of information accidentally while still leaving the possibility to define arbitrary keys.
///
public enum FileProviderLogDetailKey: String, Sendable {
    ///
    /// The identifier for an account.
    ///
    case account

    ///
    /// The raw value of an `NSFileProviderDomainIdentifier`.
    ///
    case domain

    ///
    /// The original and underlying error.
    ///
    /// Use this for any `Error` or `NSError`, the logging system will extract relevant values in a central place automatically.
    ///
    case error

    ///
    /// HTTP entity tag.
    ///
    /// See [Wikipedia](https://en.wikipedia.org/wiki/HTTP_ETag) for further information.
    ///
    case eTag

    ///
    /// The raw value of an `NSFileProviderItemIdentifier`.
    /// Also known and used as `ocId`.
    ///
    case item

    ///
    /// An `NKLock` as provided by NextcloudKit when a file system item is locked on the server.
    ///
    case lock

    ///
    /// The name of a file or directory in the file system.
    ///
    case name

    ///
    /// An `NSFileProviderRequest`.
    ///
    case request

    ///
    /// The last time item metadata was synchronized with the server.
    ///
    case syncTime

    ///
    /// Any relevant URL, in example in context of a network request.
    ///
    case url
}

extension FileProviderLogDetailKey: Comparable {
    public static func < (lhs: FileProviderLogDetailKey, rhs: FileProviderLogDetailKey) -> Bool {
        lhs.rawValue < rhs.rawValue
    }
}
