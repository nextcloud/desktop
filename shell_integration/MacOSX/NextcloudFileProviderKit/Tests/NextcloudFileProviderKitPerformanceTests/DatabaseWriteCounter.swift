//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB
@testable import NextcloudFileProviderKit

///
/// Counts what a ``FilesDatabaseManager`` writes, through its internal GRDB writer.
///
final class DatabaseWriteCounter: TransactionObserver, @unchecked Sendable {
    private let lock = NSLock()
    private var commitCount = 0
    private let writer: any DatabaseWriter

    init(_ manager: FilesDatabaseManager) {
        writer = manager.writer
        writer.add(transactionObserver: self, extent: .observerLifetime)
    }

    /// Write transactions committed since the counter was attached.
    var commits: Int {
        lock.withLock { commitCount }
    }

    ///
    /// Rows inserted, updated or deleted through the writer's connection since it opened. Waits for a write in progress.
    ///
    func rowChanges() -> Int {
        writer.writeWithoutTransaction { $0.totalChangesCount }
    }

    /// Changes are not observed, but GRDB reports every commit to every observer.
    func observes(eventsOfKind _: DatabaseEventKind) -> Bool {
        false
    }

    func databaseDidChange(with _: DatabaseEvent) {}

    func databaseDidCommit(_: Database) {
        lock.withLock { commitCount += 1 }
    }

    func databaseDidRollback(_: Database) {}
}
