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

        // The item rows this session describes are already written; a lost session would leave them in place with nothing to deliver.
        return write("Could not persist a change-delivery session.") { db in
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

    ///
    /// Return the active delivery session which `anchorKey` continues, either as its committed anchor or as the anchor of its reported but not yet acknowledged batch, or `nil` when there is none.
    ///
    /// Both are matched in one query and read from the same snapshot, because a concurrent acknowledgement moves the anchor from one to the other and two separate queries could miss it in both.
    ///
    /// Throws when the database could not be read, which the caller must not take for an absent session: finishing on that would acknowledge and discard changes which were never delivered.
    ///
    func changeDeliverySession(forContinuationKey anchorKey: String, containerKey: String) throws -> ChangeDeliverySessionState? {
        try writer.read { db in
            try ChangeDeliverySessionRecord
                .filter(
                    ChangeDeliverySessionRecord.Columns.containerKey == containerKey
                        && ChangeDeliverySessionRecord.Columns.completed == false
                        && (
                            ChangeDeliverySessionRecord.Columns.currentAnchorKey == anchorKey
                                || (
                                    ChangeDeliverySessionRecord.Columns.pendingAnchorKey == anchorKey
                                        && ChangeDeliverySessionRecord.Columns.pendingReported == true
                                )
                        )
                )
                .order(ChangeDeliverySessionRecord.Columns.sessionId)
                .fetchOne(db)?
                .asTuple
        }
    }

    /// Return an active delivery session by its durable identifier, or `nil` when there is none; throws like ``changeDeliverySession(forContinuationKey:containerKey:)``.
    func changeDeliverySession(sessionId: String) throws -> ChangeDeliverySessionState? {
        try writer.read { db in
            try activeChangeDeliverySession(sessionId: sessionId, in: db)?.asTuple
        }
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

    ///
    /// Mark the next batch of at most `maxItems` items as reported and return it. If a batch is already reported but not acknowledged, return that batch again so a repeated request replays it.
    ///
    /// The session is read and changed in one write transaction, so a concurrent acknowledgement cannot leave the batch starting at a stale position.
    ///
    /// - Parameter continuationAnchorKey: The anchor key for a batch which ends before the given sequence and is followed by more.
    /// - Returns: `nil` when the session no longer exists. Throws when the database could not be read or written; the session is left as it is.
    ///
    func prepareChangeDeliveryBatch(
        sessionId: String,
        maxItems: Int,
        continuationAnchorKey: (Int) -> String
    ) throws -> (items: [(metadataData: Data, deleted: Bool)], endSequence: Int, nextAnchorKey: String?, moreComing: Bool)? {
        try write(durability: .relaxed) { db in
            guard var session = try activeChangeDeliverySession(sessionId: sessionId, in: db) else {
                return nil
            }

            if !session.pendingReported {
                let candidates = try changeDeliveryItems(sessionId: sessionId, fromSequence: session.nextSequence, limit: maxItems + 1, in: db)
                let moreComing = candidates.count > maxItems
                let endSequence = candidates.prefix(maxItems).last.map { $0.sequence + 1 } ?? session.nextSequence

                session.pendingEndSequence = endSequence
                session.pendingAnchorKey = moreComing ? continuationAnchorKey(endSequence) : nil
                session.pendingMoreComing = moreComing
                session.pendingReported = true
                try session.update(db)
            }

            let items = try changeDeliveryItems(
                sessionId: sessionId,
                fromSequence: session.nextSequence,
                limit: session.pendingEndSequence - session.nextSequence,
                in: db
            ).map { (metadataData: $0.metadataData, deleted: $0.deleted) }

            return (items, session.pendingEndSequence, session.pendingAnchorKey, session.pendingMoreComing)
        }
    }

    ///
    /// Acknowledge the reported batch which ends before `endSequence` after the observer accepted it.
    ///
    /// The batch is identified explicitly because the next request can acknowledge it first and prepare the following batch before the enumerator which reported it gets here. The session is read inside the write transaction so such a concurrent change is seen.
    ///
    /// - Returns: `false` if the batch cannot be acknowledged or the write fails. A batch which is no longer reported, for example because it was acknowledged already, is ignored and `true` is returned.
    ///
    func acknowledgeChangeDeliveryBatch(sessionId: String, endSequence: Int) -> Bool {
        write("Could not acknowledge a change-delivery batch.", durability: .relaxed) { db in
            guard var session = try activeChangeDeliverySession(sessionId: sessionId, in: db),
                  session.pendingReported,
                  session.pendingEndSequence == endSequence
            else {
                return true
            }

            if session.pendingMoreComing, session.pendingAnchorKey == nil {
                return false
            }

            let decoder = JSONDecoder()
            let deletedOcIds = try changeDeliveryItems(
                sessionId: sessionId,
                fromSequence: session.nextSequence,
                limit: endSequence - session.nextSequence,
                in: db
            ).compactMap { item in
                item.deleted ? try? decoder.decode(SendableItemMetadata.self, from: item.metadataData).ocId : nil
            }

            for chunk in deletedOcIds.chunked(into: Self.inClauseChunkSize) {
                let deletedItems = ItemMetadataRecord.filter(chunk.contains(ItemMetadataRecord.Columns.ocId))

                if session.hardRemoveDeleted {
                    try deletedItems.deleteAll(db)
                } else {
                    try deletedItems.updateAll(db, ItemMetadataRecord.Columns.deleted.set(to: true))
                }
            }

            if session.pendingMoreComing, let pendingAnchorKey = session.pendingAnchorKey {
                try ChangeDeliveryItemRecord
                    .filter(
                        ChangeDeliveryItemRecord.Columns.sessionId == sessionId
                            && ChangeDeliveryItemRecord.Columns.sequence < endSequence
                    )
                    .deleteAll(db)
                session.nextSequence = endSequence
                session.currentAnchorKey = pendingAnchorKey
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
