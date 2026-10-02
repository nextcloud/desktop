//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

extension Item {
    ///
    /// Gets all MIME type filters from the server capabilities for comparison.
    ///
    /// - Parameters:
    ///     - account: The account identifier for the server to check.
    ///     - remoteInterface: The server proxy object to use.
    ///     - taskHandler: Receives the network task when capabilities must be fetched.
    ///
    /// - Returns: An array of strings as provided by NextcloudCapabilitiesKit or an empty array in case of error.
    ///
    static func getContextMenuItemTypeFilters(
        account: Account,
        remoteInterface: RemoteInterface,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> [String] {
        let (_, capabilities, _, capabilitiesError) = await remoteInterface.currentCapabilities(account: account, options: .init(), taskHandler: taskHandler)

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
