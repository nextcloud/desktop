// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
import os

/// Binds a download to its item and distinguishes supersession from caller cancellation.
struct DownloadOperation: Sendable {
    private(set) var ocId: String
    /// Identifies this attempt so the database can reject writes from an older download of the same item.
    private(set) var identifier = UUID()
    /// Stops the fetch's Swift task and registered network requests, including those for folder children.
    let cancellation: NetworkOperationCancellation
    private let logger: FileProviderLogger
    private let superseded = OSAllocatedUnfairLock(initialState: false)

    init(ocId: String, cancellation: NetworkOperationCancellation, log: any FileProviderLogging) {
        self.ocId = ocId
        self.cancellation = cancellation
        logger = FileProviderLogger(category: "DownloadOperation", log: log)
    }

    /// Gives each child its own item identity and ownership UUID for database updates.
    /// The cancellation handler and superseded flag remain shared so replacing a child stops the whole folder fetch.
    func childOperation(for ocId: String) -> Self {
        var child = self
        child.ocId = ocId
        child.identifier = UUID()
        return child
    }

    /// Stops an older fetch after a newer fetch takes ownership of its item.
    func supersede() {
        if !cancellation.isCancelled {
            superseded.withLock { $0 = true }
            logger.debug("Stopping superseded download.", [.item: ocId])
        }
        cancellation.cancel()
    }

    /// Reports ownership loss as a synchronization failure; otherwise preserves the original error.
    func error(for error: Error) -> Error {
        superseded.withLock { $0 } ? NSFileProviderError(.cannotSynchronize) : error
    }
}
