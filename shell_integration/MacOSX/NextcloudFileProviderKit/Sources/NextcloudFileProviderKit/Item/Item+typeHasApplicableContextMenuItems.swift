//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

extension Item {
    ///
    /// Convenience wrapper for ``getContextMenuItemTypeFilters(account:remoteInterface:)`` and ``typeHasApplicableContextMenuItems(filters:candidate:)``.
    ///
    /// Depending on the call site, it might be more efficient to call both methods individually to avoid redundant capability checks or circumvent boundaries of synchronous and asynchronous contexts.
    ///
    /// - Parameters:
    ///     - candidate: The MIME type of the file provider item to check.
    ///
    /// - Returns: `true`, if the candidate MIME type is covered by the list of filters provided, otherwise `false`.
    ///
    static func typeHasApplicableContextMenuItems(account: Account, remoteInterface: RemoteInterface, candidate: String) async -> Bool {
        let filters = await getContextMenuItemTypeFilters(account: account, remoteInterface: remoteInterface)
        return typeHasApplicableContextMenuItems(filters: filters, candidate: candidate)
    }

    ///
    /// Check whether the MIME type of an item matches any type filter of server-defined context menu items.
    ///
    /// - Parameters:
    ///     - filters: A list of MIME type filter strings and prefixes as provided by the server.
    ///     - candidate: The MIME type of the file provider item to check.
    ///
    /// - Returns: `true`, if the candidate MIME type is covered by the list of filters provided, otherwise `false`.
    ///
    static func typeHasApplicableContextMenuItems(filters: [String], candidate: String) -> Bool {
        filters.first(where: { candidate.hasPrefix($0) }) != nil
    }
}
