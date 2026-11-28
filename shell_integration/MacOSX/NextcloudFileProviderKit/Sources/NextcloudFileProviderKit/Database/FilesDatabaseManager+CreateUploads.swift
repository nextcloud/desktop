// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

extension FilesDatabaseManager {
    func pendingCreateUpload(itemIdentifier: String) -> PendingCreateUploadRecord? {
        let cutoff = Date().timeIntervalSince1970 - PendingCreateUploadRecord.retention
        return read("Could not read the pending create upload.", [.item: itemIdentifier]) { db in
            try PendingCreateUploadRecord
                .filter(sql: "account = ? AND itemIdentifier = ? AND startedAt >= ?", arguments: [account.ncKitAccount, itemIdentifier, cutoff])
                .fetchOne(db)
        } ?? nil
    }

    func recordPendingCreateUpload(_ attempt: PendingCreateUploadRecord) -> Bool {
        let cutoff = attempt.startedAt - PendingCreateUploadRecord.retention
        return write("Could not record the pending create upload.", [.item: attempt.itemIdentifier]) { db in
            _ = try PendingCreateUploadRecord.filter(Column("startedAt") < cutoff).deleteAll(db)
            try attempt.upsert(db)
            return true
        } ?? false
    }

    func discardPendingCreateUpload(_ attempt: PendingCreateUploadRecord) {
        write("Could not discard the pending create upload.", [.item: attempt.itemIdentifier]) { db in
            _ = try PendingCreateUploadRecord.filter(Column("attemptIdentifier") == attempt.attemptIdentifier).deleteAll(db)
        }
    }

    func saveCreatedFile(_ metadata: SendableItemMetadata, attempt: PendingCreateUploadRecord?) -> Bool {
        write("Could not save the created file.", [.item: metadata.ocId]) { db in
            try insertItemMetadata(metadata, in: db)
            if let attempt {
                _ = try PendingCreateUploadRecord.filter(Column("attemptIdentifier") == attempt.attemptIdentifier).deleteAll(db)
            }
            return true
        } ?? false
    }
}
