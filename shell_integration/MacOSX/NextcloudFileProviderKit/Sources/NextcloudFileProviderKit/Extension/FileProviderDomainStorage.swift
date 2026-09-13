//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

/// Selects the directory used for File Provider sync metadata.
///
/// External-volume domains keep their sync state in File Provider's state directory on
/// that volume. Keeping the Realm database there makes the metadata follow volume snapshots
/// instead of diverging from restored file contents.
enum FileProviderDomainStorage {
    static func databaseDirectory(
        volumeUUID: UUID?,
        stateDirectory: () throws -> URL,
        accessStateDirectory: (URL) throws -> Void
    ) throws -> URL? {
        guard volumeUUID != nil else {
            return nil
        }

        let directory = try stateDirectory()
        try accessStateDirectory(directory)
        return directory
    }
}
