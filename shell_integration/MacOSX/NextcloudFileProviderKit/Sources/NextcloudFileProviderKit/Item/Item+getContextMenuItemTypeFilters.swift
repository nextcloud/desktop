//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

extension Item {
    ///
    /// Gets all MIME type filters from the server capabilities for comparison.
    ///
    /// - Parameters:
    ///     - account: The account identifier for the server to check.
    ///     - remoteInterface: The server proxy object to use.
    ///
    /// - Returns: An array of strings as provided by NextcloudCapabilitiesKit or an empty array in case of error.
    ///
    static func getContextMenuItemTypeFilters(account: Account, remoteInterface: RemoteInterface) async -> [String] {
        let (_, capabilities, _, capabilitiesError) = await remoteInterface.currentCapabilities(account: account, options: .init(), taskHandler: { _ in })

        if capabilitiesError == .success {
            if let capabilities {
                if let apps = capabilities.clientIntegration?.apps {
                    return apps.flatMap(\.contextMenuItems).flatMap(\.filters)
                }
            }
        }

        return []
    }
}
