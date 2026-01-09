//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import CoreData
import FileProvider

extension Database: DatabaseManaging {
    func getItem(by identifier: NSFileProviderItemIdentifier) -> SendableItem? {
        let signpostID = signposter.makeSignpostID()
        let interval = signposter.beginInterval("getItemByIdentifier", id: signpostID)

        defer {
            signposter.endInterval("getItemByIdentifier", interval)
        }

        return nil // TODO
    }

    func insertItem() throws {
        let signpostID = signposter.makeSignpostID()
        let interval = signposter.beginInterval("inserItem", id: signpostID)

        defer {
            signposter.endInterval("insertItem", interval)
        }

        let context = persistentContainer.newBackgroundContext()
        let item = DatabaseItem(context: context)
        context.insert(item)

        do {
            try context.save()
        } catch {
            logger.error("Failed to inser item: \(error.localizedDescription)")
            throw error
        }
    }
}
