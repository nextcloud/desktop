//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

/// A path below the account's Files or Trash WebDAV root.
struct DAVPath: Equatable, Sendable {
    let root: DAVRoot
    /// The part after the DAV root URL, including the leading slash when present.
    /// For example, `/Team/Docs` under Files, or an empty string for the root itself.
    let path: String

    var isRoot: Bool { path.isEmpty || path == "/" }
    var isFilesRoot: Bool { root == .files && isRoot }
}
