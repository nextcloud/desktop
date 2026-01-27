//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider

///
/// Requirements for types which manage remote item metadata or mocks of such.
///
public protocol DatabaseManaging: Actor {
    func insert(_ item: FileProviderItem) throws

    ///
    /// Fetch the item by its file provider item identifier.
    ///
    /// - Parameters:
    ///     - identifier: Any valid file provider item identifier.
    ///
    /// - Throws: ``DatabaseError.databaseItemNotFound`` in case no item was found because when calling this the usual assumption is that such an item exists.
    ///
    func item(by identifier: NSFileProviderItemIdentifier) throws -> FileProviderItem

    ///
    /// Fetch an item by its remote identifier.
    ///
    /// - Parameters:
    ///     - identifier: This refers to the `ocID` property which is the unique item identifier on the server side.
    ///
    /// - Throws: ``DatabaseError.databaseItemNotFound`` in case no item was found because when calling this the usual assumption is that such an item exists.
    ///
    func item(byRemoteIdentifier identifier: String) throws -> FileProviderItem

    func materializedItems() throws -> [FileProviderItem]
}
