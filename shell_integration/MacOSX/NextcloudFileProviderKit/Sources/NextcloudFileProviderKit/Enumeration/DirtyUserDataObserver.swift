//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

///
/// An enumeration observer for materialized items which reports the existence of dirty user data to the completion handler.
///
public final class DirtyUserDataObserver: NSObject, NSFileProviderEnumerationObserver {
    private let completionHandler: (Bool) -> Void
    private var hasDirtyUserData = false
    private var logger: FileProviderLogger

    ///
    /// - Parameters:
    ///     - completionHandler: The handler to call for on completion of the enumeration. The provided boolean indicates whether dirty user data exists or not.
    ///
    public init(log: any FileProviderLogging, completionHandler: @escaping (Bool) -> Void) {
        self.completionHandler = completionHandler
        logger = FileProviderLogger(category: "DirtyUserDataObserver", log: log)
        super.init()
        logger.debug("Initialized.")
    }

    public func didEnumerate(_ updatedItems: [any NSFileProviderItemProtocol]) {
        logger.debug("Did enumerate \(updatedItems.count) items.")

        for item in updatedItems {
            guard item.itemIdentifier != .rootContainer else {
                continue
            }

            guard item.itemIdentifier != .trashContainer else {
                continue
            }

            guard item.itemIdentifier != .workingSet else {
                continue
            }

            if item.isUploaded == false {
                logger.info("Found item which is not uploaded yet!", [.item: item.itemIdentifier, .name: item.filename])
                hasDirtyUserData = true
                return
            }
        }
    }

    public func finishEnumerating(upTo _: NSFileProviderPage?) {
        logger.info("Finished enumerating. \(hasDirtyUserData ? "Dirty user data found." : "No dirty user data found.")")
        completionHandler(hasDirtyUserData)
    }

    public func finishEnumeratingWithError(_ error: any Error) {
        logger.error("Finished enumerating with error. \(hasDirtyUserData ? "Dirty user data found." : "No dirty user data found.")", [.error: error])
        completionHandler(hasDirtyUserData)
    }
}
