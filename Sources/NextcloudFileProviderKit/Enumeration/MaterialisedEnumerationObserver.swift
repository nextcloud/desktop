/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import FileProvider
import Foundation
import OSLog
import RealmSwift

public class MaterialisedEnumerationObserver: NSObject, NSFileProviderEnumerationObserver {
    static let logger = Logger(subsystem: Logger.subsystem, category: "materialisedobservation")
    public let ncKitAccount: String
    let dbManager: FilesDatabaseManager
    private let completionHandler: (
        _ materialisedIds: Set<String>, _ unmaterialisedIds: Set<String>
    ) -> Void
    private var allEnumeratedItemIds = Set<String>()

    public required init(
        ncKitAccount: String,
        dbManager: FilesDatabaseManager,
        completionHandler: @escaping (
            _ materialisedIds: Set<String>, _ unmaterialisedIds: Set<String>
        ) -> Void
    ) {
        self.ncKitAccount = ncKitAccount
        self.dbManager = dbManager
        self.completionHandler = completionHandler
        super.init()
    }

    public func didEnumerate(_ updatedItems: [NSFileProviderItemProtocol]) {
        updatedItems.map(\.itemIdentifier.rawValue).forEach { allEnumeratedItemIds.insert($0) }
    }

    public func finishEnumerating(upTo _: NSFileProviderPage?) {
        Self.logger.debug("Handling enumerated materialised items.")
        Self.handleEnumeratedItems(
            allEnumeratedItemIds,
            account: ncKitAccount,
            dbManager: dbManager,
            completionHandler: completionHandler
        )
    }

    public func finishEnumeratingWithError(_ error: Error) {
        Self.logger.error(
            "Ran into error when enumerating materialised items: \(error.localizedDescription, privacy: .public). Handling items enumerated so far"
        )
        Self.handleEnumeratedItems(
            allEnumeratedItemIds,
            account: ncKitAccount,
            dbManager: dbManager,
            completionHandler: completionHandler
        )
    }

    static func handleEnumeratedItems(
        _ itemIds: Set<String>,
        account: String,
        dbManager: FilesDatabaseManager,
        completionHandler: @escaping (
            _ materialisedIds: Set<String>, _ unmaterialisedIds: Set<String>
        ) -> Void
    ) {
        let materialisedMetadatas = dbManager.materialisedItemMetadatas(account: account)
        var materialisedMetadatasMap = [String: SendableItemMetadata]()
        var unmaterialisedIds = Set<String>()
        var newMaterialisedIds = Set<String>()

        materialisedMetadatas.forEach {
            materialisedMetadatasMap[$0.ocId] = $0
            unmaterialisedIds.insert($0.ocId)
        }

        for enumeratedId in itemIds {
            if unmaterialisedIds.contains(enumeratedId) {
                unmaterialisedIds.remove(enumeratedId)
            } else {
                newMaterialisedIds.insert(enumeratedId)
                guard var metadata = dbManager.itemMetadata(ocId: enumeratedId) else {
                    Self.logger.error("No metadata for \(enumeratedId, privacy: .public) found")
                    continue
                }
                if metadata.directory {
                    metadata.visitedDirectory = true
                } else {
                    metadata.downloaded = true
                }
                dbManager.addItemMetadata(metadata)
            }
        }

        for unmaterialisedId in unmaterialisedIds {
            guard var metadata = materialisedMetadatasMap[unmaterialisedId] else {
                Self.logger.error("No materialised for \(unmaterialisedId, privacy: .public) found")
                continue
            }
            metadata.downloaded = false
            metadata.visitedDirectory = false
            dbManager.addItemMetadata(metadata)
        }

        // TODO: Do we need to signal the working set now? Unclear

        completionHandler(newMaterialisedIds, unmaterialisedIds)
    }
}
