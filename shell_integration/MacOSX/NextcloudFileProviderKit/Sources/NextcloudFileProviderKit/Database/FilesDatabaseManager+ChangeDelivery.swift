//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

extension FilesDatabaseManager {
    /// Create a durable change-delivery session and store the complete ordered change snapshot.
    func createChangeDeliverySession(
        sessionId: String,
        anchorKey: String,
        finalAnchorRawValue: Data,
        updated: [SendableItemMetadata],
        deleted: [SendableItemMetadata],
        incomplete: Bool
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
                    currentAnchorKey: anchorKey,
                    finalAnchorRawValue: finalAnchorRawValue,
                    incomplete: incomplete
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
    /// Append more changes to an open session, after everything already queued.
    ///
    /// Streaming delivery primes a session from a scan's first wave and appends the rest as they are
    /// discovered, so a change found early is reported without waiting for the whole walk. Sequences
    /// continue past the highest one stored — consumed items are deleted as the drain advances, so
    /// the session's `nextSequence` is the floor to build on rather than the stored count.
    ///
    func appendChangeDeliveryItems(
        sessionId: String,
        updated: [SendableItemMetadata],
        deleted: [SendableItemMetadata]
    ) -> Bool {
        let encoder = JSONEncoder()
        let allChanges: [(metadata: SendableItemMetadata, deleted: Bool)] =
            updated.map { (metadata: $0, deleted: false) } + deleted.map { (metadata: $0, deleted: true) }

        guard !allChanges.isEmpty else { return true }

        let encodedChanges: [(data: Data, deleted: Bool)] = allChanges.compactMap { change in
            guard let metadataData = try? encoder.encode(change.metadata) else { return nil }
            return (data: metadataData, deleted: change.deleted)
        }

        guard encodedChanges.count == allChanges.count else {
            logger.error("Could not encode all metadata appended to a change-delivery session.")
            return false
        }

        let database = ncDatabase()

        guard let session = database.object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId),
              !session.completed
        else {
            logger.error("Cannot append to a change-delivery session that is not open.")
            return false
        }

        let highestStored = database
            .objects(RealmChangeDeliveryItem.self)
            .filter("sessionId == %@", sessionId)
            .max(ofProperty: "sequence") as Int? ?? (session.nextSequence - 1)
        let firstSequence = max(highestStored + 1, session.nextSequence)

        do {
            try database.write {
                let items = encodedChanges.enumerated().map { offset, change in
                    RealmChangeDeliveryItem(
                        sessionId: sessionId,
                        sequence: firstSequence + offset,
                        metadataData: change.data,
                        deleted: change.deleted
                    )
                }
                database.add(items, update: .modified)
            }
            return true
        } catch {
            logger.error("Could not append to a change-delivery session.")
            return false
        }
    }

    ///
    /// Record a streaming session's outcome once its producer has finished.
    ///
    /// A streaming session is primed `incomplete` because the scan's success is not yet known. A
    /// clean finish stamps the real sync anchor and clears the flag, which is what lets the
    /// working-set sync point advance. Left as primed, the session keeps its incoming anchor and the
    /// next signal re-derives — the safe outcome when a scan fails or is abandoned.
    ///
    func finalizeChangeDeliverySession(
        sessionId: String,
        finalAnchorRawValue: Data,
        incomplete: Bool
    ) {
        let database = ncDatabase()

        guard let session = database.object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId),
              !session.completed
        else {
            return
        }

        try? database.write {
            session.finalAnchorRawValue = finalAnchorRawValue
            session.incomplete = incomplete
        }
    }

    /// Return the active delivery session whose current continuation anchor matches `anchorKey`.
    func changeDeliverySession(forAnchorKey anchorKey: String) -> (
        sessionId: String,
        currentAnchorKey: String,
        nextSequence: Int,
        finalAnchorRawValue: Data,
        incomplete: Bool
    )? {
        let sessions = ncDatabase()
            .objects(RealmChangeDeliverySession.self)
            .filter("currentAnchorKey == %@ AND completed == false", anchorKey)
        guard let session = sessions.first
        else {
            return nil
        }

        return changeDeliverySession(sessionId: session.sessionId)
    }

    /// Return an active delivery session by its durable identifier.
    func changeDeliverySession(sessionId: String) -> (
        sessionId: String,
        currentAnchorKey: String,
        nextSequence: Int,
        finalAnchorRawValue: Data,
        incomplete: Bool
    )? {
        guard let session = ncDatabase().object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId),
              !session.completed
        else {
            return nil
        }

        return (
            session.sessionId,
            session.currentAnchorKey,
            session.nextSequence,
            session.finalAnchorRawValue,
            session.incomplete
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

    /// Advance an active session to its next continuation anchor, or remove it after the final batch.
    func advanceChangeDeliverySession(
        sessionId: String,
        nextSequence: Int,
        nextAnchorKey: String?,
        completed: Bool
    ) {
        let database = ncDatabase()
        guard let session = database.object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId) else {
            return
        }

        try? database.write {
            if completed {
                database.objects(RealmChangeDeliveryItem.self)
                    .filter("sessionId == %@", sessionId)
                    .forEach { database.delete($0) }
                database.delete(session)
            } else {
                session.nextSequence = nextSequence
                session.completed = false
                if let nextAnchorKey {
                    session.currentAnchorKey = nextAnchorKey
                }

                database.objects(RealmChangeDeliveryItem.self)
                    .filter("sessionId == %@ AND sequence < %@", sessionId, nextSequence)
                    .forEach { database.delete($0) }
            }
        }
    }

    /// Remove an abandoned change-delivery session and all of its queued items.
    func removeChangeDeliverySession(sessionId: String) {
        let database = ncDatabase()
        guard let session = database.object(ofType: RealmChangeDeliverySession.self, forPrimaryKey: sessionId) else {
            return
        }

        try? database.write {
            database.objects(RealmChangeDeliveryItem.self)
                .filter("sessionId == %@", sessionId)
                .forEach { database.delete($0) }
            database.delete(session)
        }
    }
}
