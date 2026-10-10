//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import GRDB

extension FilesDatabaseManager {
    typealias ChangeDeliverySessionState = (
        sessionId: String,
        containerKey: String,
        currentAnchorKey: String,
        nextSequence: Int,
        finalAnchorRawValue: Data,
        incomplete: Bool,
        pendingEndSequence: Int,
        pendingAnchorKey: String?,
        pendingMoreComing: Bool,
        pendingReported: Bool,
        hardRemoveDeleted: Bool
    )

    /// Create a durable change-delivery session and store the complete ordered change snapshot.
    func createChangeDeliverySession(
        sessionId: String,
        containerKey: String,
        anchorKey: String,
        finalAnchorRawValue: Data,
        updated: [SendableItemMetadata],
        deleted: [SendableItemMetadata],
        incomplete: Bool,
        hardRemoveDeleted: Bool
    ) -> Bool {
        let encoder = JSONEncoder()
        let allChanges: [(metadata: SendableItemMetadata, deleted: Bool)] =
            updated.map { (metadata: $0, deleted: false) } + deleted.map { (metadata: $0, deleted: true) }
        let encodedChanges: [(data: Data, deleted: Bool)] = allChanges.compactMap { change in
            guard let metadataData = try? encoder.encode(change.metadata) else {
                return nil
            }
            return (data: metadataData, deleted: change.deleted)
        }

        guard encodedChanges.count == allChanges.count else {
            logger.error("Could not encode all metadata for a change-delivery session.")
            return false
        }

        // Delivery sessions are replayable; losing one costs a repeated scan, not state.
        return write("Could not persist a change-delivery session.", durability: .relaxed) { db in
            try ChangeDeliverySessionRecord(
                sessionId: sessionId,
                containerKey: containerKey,
                currentAnchorKey: anchorKey,
                finalAnchorRawValue: finalAnchorRawValue,
                incomplete: incomplete,
                hardRemoveDeleted: hardRemoveDeleted
            ).upsert(db)

            for (index, change) in encodedChanges.enumerated() {
                try ChangeDeliveryItemRecord(
                    sessionId: sessionId,
                    sequence: index,
                    metadataData: change.data,
                    deleted: change.deleted
                ).upsert(db)
            }

            return true
        } ?? false
    }

    /// Return the active delivery session whose committed anchor matches `anchorKey`.
    func changeDeliverySession(forAnchorKey anchorKey: String, containerKey: String) -> ChangeDeliverySessionState? {
        read("Could not look up a change-delivery session by anchor.") { db in
            try ChangeDeliverySessionRecord
                .filter(
                    ChangeDeliverySessionRecord.Columns.currentAnchorKey == anchorKey
                        && ChangeDeliverySessionRecord.Columns.containerKey == containerKey
                        && ChangeDeliverySessionRecord.Columns.completed == false
                )
                .order(ChangeDeliverySessionRecord.Columns.sessionId)
                .fetchOne(db)?
                .asTuple
        } ?? nil
    }

    /// Return an active delivery session whose pending continuation anchor matches `anchorKey`.
    func changeDeliverySession(forPendingAnchorKey anchorKey: String, containerKey: String) -> ChangeDeliverySessionState? {
        read("Could not look up a change-delivery session by pending anchor.") { db in
            try ChangeDeliverySessionRecord
                .filter(
                    ChangeDeliverySessionRecord.Columns.pendingAnchorKey == anchorKey
                        && ChangeDeliverySessionRecord.Columns.containerKey == containerKey
                        && ChangeDeliverySessionRecord.Columns.pendingReported == true
                        && ChangeDeliverySessionRecord.Columns.completed == false
                )
                .order(ChangeDeliverySessionRecord.Columns.sessionId)
                .fetchOne(db)?
                .asTuple
        } ?? nil
    }

    /// Return an active delivery session by its durable identifier.
    func changeDeliverySession(sessionId: String) -> ChangeDeliverySessionState? {
        read("Could not look up a change-delivery session.") { db in
            try activeChangeDeliverySession(sessionId: sessionId, in: db)?.asTuple
        } ?? nil
    }

    /// Return the next ordered range of an active change-delivery session, or `nil` when the database could not be read.
    ///
    /// An empty array means the session has no further items; `nil` must not be taken for that, because finishing on it would acknowledge and discard changes which were never delivered.
    ///
    func changeDeliveryItems(
        sessionId: String,
        fromSequence sequence: Int,
        limit: Int
    ) -> [(sequence: Int, metadataData: Data, deleted: Bool)]? {
        read("Could not fetch change-delivery items.") { db in
            try changeDeliveryItems(sessionId: sessionId, fromSequence: sequence, limit: limit, in: db)
        }
    }

    /// Return the deleted item identifiers in the currently prepared batch, or `nil` when the database could not be read.
    func pendingChangeDeliveryDeletedOcIds(sessionId: String) -> [String]? {
        read("Could not fetch the pending deletions of a change-delivery session.") { db in
            guard let session = try activeChangeDeliverySession(sessionId: sessionId, in: db),
                  session.pendingReported,
                  session.pendingEndSequence > session.nextSequence
            else {
                return []
            }

            let decoder = JSONDecoder()
            return try changeDeliveryItems(
                sessionId: sessionId,
                fromSequence: session.nextSequence,
                limit: session.pendingEndSequence - session.nextSequence,
                in: db
            ).compactMap { item in
                guard item.deleted,
                      let metadata = try? decoder.decode(SendableItemMetadata.self, from: item.metadataData)
                else {
                    return nil
                }
                return metadata.ocId
            }
        }
    }

    func prepareChangeDeliveryBatch(
        sessionId: String,
        endSequence: Int,
        nextAnchorKey: String?,
        moreComing: Bool
    ) -> Bool {
        write("Could not prepare a change-delivery batch.", durability: .relaxed) { db in
            guard var session = try activeChangeDeliverySession(sessionId: sessionId, in: db) else {
                return false
            }

            if session.pendingReported {
                return session.pendingEndSequence == endSequence
                    && session.pendingAnchorKey == nextAnchorKey
                    && session.pendingMoreComing == moreComing
            }

            session.pendingEndSequence = endSequence
            session.pendingAnchorKey = nextAnchorKey
            session.pendingMoreComing = moreComing
            session.pendingReported = true
            try session.update(db)
            return true
        } ?? false
    }

    /// Acknowledge the prepared batch after the observer accepted it.
    ///
    /// Idempotent: a batch nobody prepared, or one another enumerator instance acknowledged meanwhile, leaves the session as it is and counts as acknowledged. The framework can ask for the continuation right after the observer's finish, through a fresh instance, while the reporting instance is still acknowledging; both then acknowledge the same batch, and the second must not take the continuation for an unknown anchor.
    ///
    func acknowledgeChangeDeliveryBatch(sessionId: String, deletedOcIds: [String]) -> Bool {
        write("Could not acknowledge a change-delivery batch.", durability: .relaxed) { db in
            guard var session = try activeChangeDeliverySession(sessionId: sessionId, in: db) else {
                return true
            }

            guard session.pendingReported else {
                return true
            }

            if session.pendingMoreComing, session.pendingAnchorKey == nil {
                return false
            }

            let pendingEndSequence = session.pendingEndSequence
            let pendingAnchorKey = session.pendingAnchorKey
            let pendingMoreComing = session.pendingMoreComing

            for chunk in deletedOcIds.chunked(into: Self.inClauseChunkSize) {
                let deletedItems = ItemMetadataRecord.filter(chunk.contains(ItemMetadataRecord.Columns.ocId))

                if session.hardRemoveDeleted {
                    try deletedItems.deleteAll(db)
                } else {
                    try deletedItems.updateAll(db, ItemMetadataRecord.Columns.deleted.set(to: true))
                }
            }

            if pendingMoreComing, let pendingAnchorKey {
                session.nextSequence = pendingEndSequence
                session.currentAnchorKey = pendingAnchorKey
                try ChangeDeliveryItemRecord
                    .filter(
                        ChangeDeliveryItemRecord.Columns.sessionId == sessionId
                            && ChangeDeliveryItemRecord.Columns.sequence < pendingEndSequence
                    )
                    .deleteAll(db)

                session.pendingEndSequence = 0
                session.pendingAnchorKey = nil
                session.pendingMoreComing = false
                session.pendingReported = false
                try session.update(db)
            } else {
                try ChangeDeliveryItemRecord
                    .filter(ChangeDeliveryItemRecord.Columns.sessionId == sessionId)
                    .deleteAll(db)
                try session.delete(db)
            }

            return true
        } ?? false
    }

    /// Remove all active delivery sessions for one enumerated container.
    func removeChangeDeliverySessions(containerKey: String) {
        write("Could not remove the change-delivery sessions of a container.", durability: .relaxed) { db in
            try db.execute(
                sql: """
                DELETE FROM changeDeliveryItem WHERE sessionId IN (
                    SELECT sessionId FROM changeDeliverySession WHERE containerKey = ? AND completed = 0
                )
                """,
                arguments: [containerKey]
            )
            try ChangeDeliverySessionRecord
                .filter(
                    ChangeDeliverySessionRecord.Columns.containerKey == containerKey
                        && ChangeDeliverySessionRecord.Columns.completed == false
                )
                .deleteAll(db)
        }
    }

    // MARK: - Workers

    private func activeChangeDeliverySession(sessionId: String, in db: Database) throws -> ChangeDeliverySessionRecord? {
        guard let session = try ChangeDeliverySessionRecord.fetchOne(db, key: sessionId), !session.completed else {
            return nil
        }

        return session
    }

    private func changeDeliveryItems(
        sessionId: String,
        fromSequence sequence: Int,
        limit: Int,
        in db: Database
    ) throws -> [(sequence: Int, metadataData: Data, deleted: Bool)] {
        try ChangeDeliveryItemRecord
            .filter(
                ChangeDeliveryItemRecord.Columns.sessionId == sessionId
                    && ChangeDeliveryItemRecord.Columns.sequence >= sequence
            )
            .order(ChangeDeliveryItemRecord.Columns.sequence)
            .limit(limit)
            .fetchAll(db)
            .map { ($0.sequence, $0.metadataData, $0.deleted) }
    }
}
