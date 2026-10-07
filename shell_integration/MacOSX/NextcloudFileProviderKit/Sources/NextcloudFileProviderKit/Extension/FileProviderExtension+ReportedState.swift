//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider

extension FileProviderExtension {
    ///
    /// Record the item a successful `createItem` or `modifyItem` reply hands to the system.
    ///
    /// Only a real ``Item`` returned without an error counts; a template fallback, an error reply and the synthesised root and trash containers do not. The reply is the state the system stores for the item, so the working-set derivation must not report it again.
    ///
    static func markReported(_ item: NSFileProviderItem?, error: Error?, dbManager: FilesDatabaseManager?) {
        guard error == nil,
              let item = item as? Item,
              item.itemIdentifier != .rootContainer,
              item.itemIdentifier != .trashContainer
        else {
            return
        }

        dbManager?.markReported([item.metadata])
    }
}
