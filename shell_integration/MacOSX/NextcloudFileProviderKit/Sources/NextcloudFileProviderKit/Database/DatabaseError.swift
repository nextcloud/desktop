//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

public enum DatabaseError: Error {
    ///
    /// No ``DatabaseItem`` with the given file provider item identifier was found in the store.
    ///
    case databaseItemNotFound(NSFileProviderItemIdentifier)
}
