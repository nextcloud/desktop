//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import CoreData
import FileProvider
import os

///
/// Default implementation of ``DatabaseManaging``.
///
/// It features extensive logging based on ``FileProviderLogging`` and records performance data through native signposts which can be inspected in Instruments.
///
actor Database {
    let logger: FileProviderLogger
    let signposter: OSSignposter

    lazy var persistentContainer: NSPersistentContainer = {
        let container = NSPersistentContainer(name: "Database")

        container.loadPersistentStores { _, error in
            if let error {
                self.logger.fault("Failed to load persistent stores: \(error.localizedDescription)")
            }
        }

        return container
    }()

    public init(log: any FileProviderLogging) {
        logger = FileProviderLogger(category: "Database", log: log)
        signposter = OSSignposter(subsystem: Bundle.main.bundleIdentifier!, category: "Database")
    }
}
