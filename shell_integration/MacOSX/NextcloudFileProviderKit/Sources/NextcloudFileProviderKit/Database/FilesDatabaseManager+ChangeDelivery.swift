//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import RealmSwift

/// The stored state of one change-delivery session.
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

extension FilesDatabaseManager {
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

        let database = ncDatabase()
        do {
            try database.write {
                let session = RealmChangeDeliverySession(
                    sessionId: sessionId,
                    containerKey: containerKey,
                    currentAnchorKey: anchorKey,
                    finalAnchorRawValue: finalAnchorRawValue,
                    incomplete: incomplete,
                    hardRemoveDeleted: hardRemoveDeleted
                )
                database.add(session, update: .modified)

                let items = encodedChanges.enumerated().map { index, change in
                    RealmChangeDeliveryItem(
                        sessionId: sessionId,
                        sequence: index,
                        metadataData: change.data,
                        deleted: change.deleted
                    )
                }
                database.add(items, update: .modified)
            }
            return true
        } catch {
            logger.error("Could not persist a change-delivery session.")
            return false
        }
    }

    ///
    /// Return the active delivery session which `anchorKey` continues, either as its committed anchor or as the
    /// anchor of its reported but not yet acknowledged batch.
    ///
    /// Both are matched in one query and read from the same snapshot, because a concurrent acknowledgement moves
    /// the anchor from one to the other and two separate queries could miss it in both.
    ///
    func changeDeliverySession(forContinuationKey anchorKey: String, containerKey: String) -> ChangeDeliverySessionState? {
        let session = ncDatabase()
            .objects(RealmChangeDeliverySession.self)
            .filter(
                "containerKey == %@ AND completed == false AND (currentAnchorKey == %@ OR (pendingAnchorKey == %@ AND pendingReported == true))",
                containerKey,
                anchorKey,
                anchorKey
            )
            .first

        return session.map(state(of:))
    }

    /// Return an active delivery session by its durable identifier.
    func changeDeliverySession(sessionId: String) -> ChangeDeliverySessionState? {
        guard let session = ncDatabase().object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId),
              !session.completed
        else {
            return nil
        }

        return state(of: session)
    }

    ///
    /// Mark the next batch of at most `maxItems` items as reported and return it. If a batch is already reported
    /// but not acknowledged, return that batch again so a repeated request replays it.
    ///
    /// The session is read and changed in one write transaction, so a concurrent acknowledgement cannot leave the
    /// batch starting at a stale position.
    ///
    /// - Parameter continuationAnchorKey: The anchor key for a batch which ends before the given sequence and is
    ///   followed by more.
    /// - Returns: `nil` if the session no longer exists or the write fails.
    ///
    func prepareChangeDeliveryBatch(
        sessionId: String,
        maxItems: Int,
        continuationAnchorKey: (Int) -> String
    ) -> (items: [(metadataData: Data, deleted: Bool)], endSequence: Int, nextAnchorKey: String?, moreComing: Bool)? {
        let database = ncDatabase()
        var batch: (items: [(metadataData: Data, deleted: Bool)], endSequence: Int, nextAnchorKey: String?, moreComing: Bool)?

        do {
            try database.write {
                guard let session = database.object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId),
                      !session.completed
                else {
                    return
                }

                if !session.pendingReported {
                    let candidates = database.objects(RealmChangeDeliveryItem.self)
                        .filter("sessionId == %@ AND sequence >= %@", sessionId, session.nextSequence)
                        .sorted(byKeyPath: "sequence")
                        .prefix(maxItems + 1)
                    let moreComing = candidates.count > maxItems
                    let endSequence = candidates.prefix(maxItems).last.map { $0.sequence + 1 } ?? session.nextSequence

                    session.pendingEndSequence = endSequence
                    session.pendingAnchorKey = moreComing ? continuationAnchorKey(endSequence) : nil
                    session.pendingMoreComing = moreComing
                    session.pendingReported = true
                }

                let items = database.objects(RealmChangeDeliveryItem.self)
                    .filter("sessionId == %@ AND sequence >= %@ AND sequence < %@", sessionId, session.nextSequence, session.pendingEndSequence)
                    .sorted(byKeyPath: "sequence")
                    .map { (metadataData: $0.metadataData, deleted: $0.deleted) }

                batch = (Array(items), session.pendingEndSequence, session.pendingAnchorKey, session.pendingMoreComing)
            }
        } catch {
            return nil
        }

        return batch
    }

    ///
    /// Acknowledge the reported batch which ends before `endSequence` after the observer accepted it.
    ///
    /// The batch is identified explicitly because the next request can acknowledge it first and prepare the
    /// following batch before the enumerator which reported it gets here. The session is read inside the write
    /// transaction so such a concurrent change is seen.
    ///
    /// - Returns: `false` if the batch cannot be acknowledged or the write fails. A batch which is no longer
    ///   reported, for example because it was acknowledged already, is ignored and `true` is returned.
    ///
    func acknowledgeChangeDeliveryBatch(sessionId: String, endSequence: Int) -> Bool {
        let database = ncDatabase()
        var acknowledged = true

        do {
            try database.write {
                guard let session = database.object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId),
                      !session.completed,
                      session.pendingReported,
                      session.pendingEndSequence == endSequence
                else {
                    return
                }

                acknowledged = acknowledgeReportedBatch(of: session, in: database)
            }
        } catch {
            return false
        }

        return acknowledged
    }

    ///
    /// Apply the deletions of the reported batch of `session`, then advance the session past it or remove the
    /// session after its final batch. Must be called inside a write transaction.
    ///
    /// - Returns: `false` if the session state is inconsistent.
    ///
    private func acknowledgeReportedBatch(of session: RealmChangeDeliverySession, in database: Realm) -> Bool {
        if session.pendingMoreComing, session.pendingAnchorKey == nil {
            return false
        }

        let sessionId = session.sessionId
        let endSequence = session.pendingEndSequence
        let decoder = JSONDecoder()
        // An explicit array type selects the eager `compactMap`, Realm needs an array for IN below.
        let deletedOcIds: [String] = database.objects(RealmChangeDeliveryItem.self)
            .filter("sessionId == %@ AND sequence >= %@ AND sequence < %@ AND deleted == true", sessionId, session.nextSequence, endSequence)
            .compactMap { try? decoder.decode(SendableItemMetadata.self, from: $0.metadataData).ocId }

        if deletedOcIds.isEmpty == false {
            let deletedItems = database.objects(RealmItemMetadata.self).filter("ocId IN %@", deletedOcIds)

            if session.hardRemoveDeleted {
                database.delete(deletedItems)
            } else {
                deletedItems.forEach { $0.deleted = true }
            }
        }

        if session.pendingMoreComing, let pendingAnchorKey = session.pendingAnchorKey {
            database.delete(
                database.objects(RealmChangeDeliveryItem.self)
                    .filter("sessionId == %@ AND sequence < %@", sessionId, endSequence)
            )
            session.nextSequence = endSequence
            session.currentAnchorKey = pendingAnchorKey
            session.pendingEndSequence = 0
            session.pendingAnchorKey = nil
            session.pendingMoreComing = false
            session.pendingReported = false
        } else {
            database.delete(database.objects(RealmChangeDeliveryItem.self).filter("sessionId == %@", sessionId))
            database.delete(session)
        }

        return true
    }

    private func state(of session: RealmChangeDeliverySession) -> ChangeDeliverySessionState {
        (
            session.sessionId,
            session.containerKey,
            session.currentAnchorKey,
            session.nextSequence,
            session.finalAnchorRawValue,
            session.incomplete,
            session.pendingEndSequence,
            session.pendingAnchorKey,
            session.pendingMoreComing,
            session.pendingReported,
            session.hardRemoveDeleted
        )
    }

    /// Remove all active delivery sessions for one enumerated container.
    func removeChangeDeliverySessions(containerKey: String) {
        let database = ncDatabase()
        let sessions = database.objects(RealmChangeDeliverySession.self)
            .filter("containerKey == %@ AND completed == false", containerKey)

        try? database.write {
            // Realm requires an array for IN, not the lazy sequence `map` returns on its results.
            let sessionIds = Array(sessions.map(\.sessionId))
            if sessionIds.isEmpty == false {
                database.objects(RealmChangeDeliveryItem.self)
                    .filter("sessionId IN %@", sessionIds)
                    .forEach { database.delete($0) }
            }
            database.delete(sessions)
        }
    }
}
