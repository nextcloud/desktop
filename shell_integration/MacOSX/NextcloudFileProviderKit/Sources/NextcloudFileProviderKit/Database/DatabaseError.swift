//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

///
/// Errors occuring on the file provider extension database level.
///
public enum DatabaseError: Error {
    ///
    /// No ``DatabaseItem`` with the given file provider item identifier was found in the store.
    ///
    case databaseItemNotFound(NSFileProviderItemIdentifier)

    ///
    /// A managed object could not be downcasted to its expected specific implementation type.
    ///
    case failedDowncast

    ///
    /// The given argument does not fulfill the requirements for the use case.
    ///
    case invalidArgument

    ///
    /// `nil` was found when a value was expected.
    ///
    case missingValue

    ///
    /// An error which was saved on the affected item in the store.
    ///
    /// - Parameters:
    ///     - localizedDescription: The original localizated description of an error.
    ///
    case persisted(localizedDescription: String)
}
