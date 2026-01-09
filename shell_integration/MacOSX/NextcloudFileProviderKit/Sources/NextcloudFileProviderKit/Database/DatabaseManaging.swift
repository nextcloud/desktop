//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider

///
/// Requirements for types which manage remote item metadata or mocks of such.
///
public protocol DatabaseManaging: Actor {
    ///
    /// Look up an item based on its file provider item identifier.
    ///
    func getItem(by identifier: NSFileProviderItemIdentifier) -> SendableItem?

    ///
    /// Add a new item to the persisted data.
    ///
    func insertItem() throws
}
