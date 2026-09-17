//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

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

    /// Return the active delivery session whose committed anchor matches `anchorKey`.
    func changeDeliverySession(forAnchorKey anchorKey: String, containerKey: String) -> (
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
    )? {
        let sessions = ncDatabase()
            .objects(RealmChangeDeliverySession.self)
            .filter("currentAnchorKey == %@ AND containerKey == %@ AND completed == false", anchorKey, containerKey)
        guard let session = sessions.first
        else {
            return nil
        }

        return changeDeliverySession(sessionId: session.sessionId)
    }

    /// Return an active delivery session whose pending continuation anchor matches `anchorKey`.
    func changeDeliverySession(forPendingAnchorKey anchorKey: String, containerKey: String) -> (
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
    )? {
        let sessions = ncDatabase()
            .objects(RealmChangeDeliverySession.self)
            .filter("pendingAnchorKey == %@ AND containerKey == %@ AND pendingReported == true AND completed == false", anchorKey, containerKey)
        guard let session = sessions.first else {
            return nil
        }

        return changeDeliverySession(sessionId: session.sessionId)
    }

    /// Return an active delivery session by its durable identifier.
    func changeDeliverySession(sessionId: String) -> (
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
    )? {
        guard let session = ncDatabase().object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId),
              !session.completed
        else {
            return nil
        }

        return (
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

    /// Return the next ordered range of an active change-delivery session.
    func changeDeliveryItems(
        sessionId: String,
        fromSequence sequence: Int,
        limit: Int
    ) -> [(sequence: Int, metadataData: Data, deleted: Bool)] {
        ncDatabase()
            .objects(RealmChangeDeliveryItem.self)
            .filter("sessionId == %@ AND sequence >= %@", sessionId, sequence)
            .sorted(byKeyPath: "sequence")
            .prefix(limit)
            .map { ($0.sequence, $0.metadataData, $0.deleted) }
    }

    /// Return the deleted item identifiers in the currently prepared batch.
    func pendingChangeDeliveryDeletedOcIds(sessionId: String) -> [String] {
        guard let session = changeDeliverySession(sessionId: sessionId),
              session.pendingReported,
              session.pendingEndSequence > session.nextSequence
        else {
            return []
        }

        let decoder = JSONDecoder()
        return changeDeliveryItems(
            sessionId: sessionId,
            fromSequence: session.nextSequence,
            limit: session.pendingEndSequence - session.nextSequence
        ).compactMap { item in
            guard item.deleted,
                  let metadata = try? decoder.decode(SendableItemMetadata.self, from: item.metadataData)
            else {
                return nil
            }
            return metadata.ocId
        }
    }

    func prepareChangeDeliveryBatch(
        sessionId: String,
        endSequence: Int,
        nextAnchorKey: String?,
        moreComing: Bool
    ) -> Bool {
        let database = ncDatabase()
        guard let session = database.object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId),
              !session.completed
        else {
            return false
        }

        if session.pendingReported {
            return session.pendingEndSequence == endSequence
                && session.pendingAnchorKey == nextAnchorKey
                && session.pendingMoreComing == moreComing
        }

        do {
            try database.write {
                session.pendingEndSequence = endSequence
                session.pendingAnchorKey = nextAnchorKey
                session.pendingMoreComing = moreComing
                session.pendingReported = true
            }
            return true
        } catch {
            return false
        }
    }

    /// Acknowledge the prepared batch after the observer accepted it.
    func acknowledgeChangeDeliveryBatch(sessionId: String, deletedOcIds: [String]) -> Bool {
        let database = ncDatabase()
        guard let session = database.object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId),
              !session.completed
        else {
            return true
        }

        guard session.pendingReported else {
            return false
        }

        if session.pendingMoreComing, session.pendingAnchorKey == nil {
            return false
        }

        let pendingEndSequence = session.pendingEndSequence
        let pendingAnchorKey = session.pendingAnchorKey
        let pendingMoreComing = session.pendingMoreComing

        do {
            try database.write {
                let deletedItems = deletedOcIds.isEmpty
                    ? database.objects(RealmItemMetadata.self).filter("ocId == %@", "")
                    : database.objects(RealmItemMetadata.self).filter("ocId IN %@", deletedOcIds)
                if session.hardRemoveDeleted {
                    database.delete(deletedItems)
                } else {
                    deletedItems.forEach { $0.deleted = true }
                }

                if pendingMoreComing, let pendingAnchorKey {
                    session.nextSequence = pendingEndSequence
                    session.currentAnchorKey = pendingAnchorKey
                    database.objects(RealmChangeDeliveryItem.self)
                        .filter("sessionId == %@ AND sequence < %@", sessionId, pendingEndSequence)
                        .forEach { database.delete($0) }
                } else {
                    database.objects(RealmChangeDeliveryItem.self)
                        .filter("sessionId == %@", sessionId)
                        .forEach { database.delete($0) }
                    database.delete(session)
                }

                if database.object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId) != nil {
                    session.pendingEndSequence = 0
                    session.pendingAnchorKey = nil
                    session.pendingMoreComing = false
                    session.pendingReported = false
                }
            }
            return true
        } catch {
            return false
        }
    }

    /// Remove all active delivery sessions for one enumerated container.
    func removeChangeDeliverySessions(containerKey: String) {
        let database = ncDatabase()
        let sessions = database.objects(RealmChangeDeliverySession.self)
            .filter("containerKey == %@ AND completed == false", containerKey)

        try? database.write {
            let sessionIds = sessions.map(\.sessionId)
            if sessionIds.isEmpty == false {
                database.objects(RealmChangeDeliveryItem.self)
                    .filter("sessionId IN %@", sessionIds)
                    .forEach { database.delete($0) }
            }
            database.delete(sessions)
        }
    }
}
