//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

extension NSFileProviderItemVersion {
    ///
    /// Initialize with strings as arguments.
    ///
    /// This will use the encoded data of the given string for the stored content and metadata version properties.
    ///
    /// - Parameters:
    ///     - entityTag: The latest entity tag of a remote item.
    ///
    convenience init(entityTag: String) {
        self.init(contentVersion: entityTag.data(using: .utf8) ?? Data(), metadataVersion: entityTag.data(using: .utf8) ?? Data())
    }

    ///
    /// Decode the `metadata` version `Data` as a `String` expected to be an entity tag.
    ///
    var entityTag: String? {
        guard let string = String(data: metadataVersion, encoding: .utf8) else {
            return nil
        }

        guard string.isEmpty == false else {
            return nil
        }

        return string
    }
}
