//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

public extension FileManager {
    ///
    /// Resolve the location of the shared container for the app group of the file provider extension.
    ///
    /// - Returns: Container URL for the extension's app group or `nil`, if it could not be found.
    ///
    func applicationGroupContainer() -> URL? {
        guard let infoDictionary = Bundle.main.infoDictionary else {
            return nil
        }

        guard let extensionDictionary = infoDictionary["NSExtension"] as? [String: Any] else {
            return nil
        }

        guard let appGroupIdentifier = extensionDictionary["NSExtensionFileProviderDocumentGroup"] as? String else {
            return nil
        }

        return containerURL(forSecurityApplicationGroupIdentifier: appGroupIdentifier)
    }
}
