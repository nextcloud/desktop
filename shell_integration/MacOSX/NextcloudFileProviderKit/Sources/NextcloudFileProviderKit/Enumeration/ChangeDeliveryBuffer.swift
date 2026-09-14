//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// Durable FIFO state for a multi-batch File Provider change enumeration.
///
/// The framework can invalidate an enumerator after an intermediate batch and invoke the next batch on a
/// new enumerator. The pending changes and their position are therefore stored in Realm. An intermediate
/// anchor identifies those pending changes and must be handled before ordinary sync-anchor validation.
///
/// Updates are stored before deletions and are already sorted parents-before-children by the caller. Each
/// batch consumes one combined item budget, so the framework never receives an oversized update/delete
/// payload.
///
/// ``Enumerator`` is `Sendable` with only immutable members, so the mutable session identifier lives behind
/// this `@unchecked Sendable`, `NSLock`-guarded box — the established concurrency idiom in this target
/// (see `FileProviderExtension.actionsLock`). `Synchronization.Mutex` is unavailable because the
/// deployment target is macOS 13.
///
final class ChangeDeliveryBuffer: @unchecked Sendable {
    private static let continuationPrefix = "fp-continuation|"

    private let lock = NSLock()
    private let dbManager: FilesDatabaseManager
    private let logger: FileProviderLogger
    private let containerKey: String
    private let hardRemoveDeleted: Bool
    private var sessionId: String?

    init(
        dbManager: FilesDatabaseManager,
        containerKey: String,
        hardRemoveDeleted: Bool,
        log: any FileProviderLogging
    ) {
        self.dbManager = dbManager
        self.containerKey = containerKey
        self.hardRemoveDeleted = hardRemoveDeleted
        logger = FileProviderLogger(category: "ChangeDeliveryBuffer", log: log)
    }

    /// Whether an active durable session is positioned at the given anchor.
    func isPrimed(forKey key: String) -> Bool {
        lock.lock()
        defer { lock.unlock() }

        if let sessionId {
            guard let session = dbManager.changeDeliverySession(sessionId: sessionId) else {
                self.sessionId = nil
                return false
            }

            if session.currentAnchorKey == key {
                return true
            }

            guard session.pendingReported,
                  session.pendingAnchorKey == key
            else {
                return false
            }

            let deletedOcIds = dbManager.pendingChangeDeliveryDeletedOcIds(sessionId: sessionId)
            guard dbManager.acknowledgeChangeDeliveryBatch(sessionId: sessionId, deletedOcIds: deletedOcIds) else {
                logger.error("Could not acknowledge the previously reported change delivery batch.")
                return false
            }

            return true
        }

        if let session = dbManager.changeDeliverySession(forAnchorKey: key, containerKey: containerKey) {
            sessionId = session.sessionId
            return true
        }

        guard let session = dbManager.changeDeliverySession(forPendingAnchorKey: key, containerKey: containerKey) else {
            return false
        }

        let deletedOcIds = dbManager.pendingChangeDeliveryDeletedOcIds(sessionId: session.sessionId)
        guard dbManager.acknowledgeChangeDeliveryBatch(sessionId: session.sessionId, deletedOcIds: deletedOcIds) else {
            logger.error("Could not acknowledge the previously reported change delivery batch.")
            return false
        }

        sessionId = session.sessionId
        return true
    }

    /// Whether `key` is an active durable continuation anchor.
    func isContinuation(forKey key: String) -> Bool {
        guard key.hasPrefix(Self.continuationPrefix) else {
            return false
        }

        return isPrimed(forKey: key)
    }

    /// Whether the currently active derivation was incomplete and must retain its incoming sync anchor.
    func isPrimedIncomplete() -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard let sessionId,
              let session = dbManager.changeDeliverySession(sessionId: sessionId)
        else {
            return false
        }

        return session.incomplete
    }

    /// Persist the complete ordered change set for a new drain sequence.
    func prime(
        key: String,
        finalAnchorRawValue: Data,
        updated: [SendableItemMetadata],
        deleted: [SendableItemMetadata],
        incomplete: Bool = false
    ) {
        lock.lock()
        defer { lock.unlock() }

        dbManager.removeChangeDeliverySessions(containerKey: containerKey)

        let newSessionId = UUID().uuidString
        guard dbManager.createChangeDeliverySession(
            sessionId: newSessionId,
            containerKey: containerKey,
            anchorKey: key,
            finalAnchorRawValue: finalAnchorRawValue,
            updated: updated,
            deleted: deleted,
            incomplete: incomplete,
            hardRemoveDeleted: hardRemoveDeleted
        ) else {
            sessionId = nil
            logger.error("Could not persist change delivery session.")
            return
        }

        sessionId = newSessionId
        logger.debug("Persisted change delivery session.")
    }

    /// Prepare the next update/delete batch without advancing the committed cursor.
    /// Returns a continuation anchor for intermediate batches and the session's final anchor for the last.
    func prepareChangeDeliveryBatch(
        maxItems: Int
    ) -> (
        updated: [SendableItemMetadata],
        deleted: [SendableItemMetadata],
        moreComing: Bool,
        continuationAnchorRawValue: Data?,
        finalAnchorRawValue: Data?
    ) {
        lock.lock()
        defer { lock.unlock() }

        let budget = max(1, maxItems)
        guard let sessionId,
              let session = dbManager.changeDeliverySession(sessionId: sessionId)
        else {
            return ([], [], false, nil, nil)
        }

        let storedItems = dbManager.changeDeliveryItems(
            sessionId: sessionId,
            fromSequence: session.nextSequence,
            limit: budget + 1
        )
        let batchItems = Array(storedItems.prefix(budget))
        let moreComing = storedItems.count > budget
        let decoder = JSONDecoder()
        var updated = [SendableItemMetadata]()
        var deleted = [SendableItemMetadata]()

        for item in batchItems {
            guard let metadata = try? decoder.decode(SendableItemMetadata.self, from: item.metadataData) else {
                logger.error("Could not decode change delivery item.")
                return ([], [], false, nil, session.finalAnchorRawValue)
            }

            if item.deleted {
                deleted.append(metadata)
            } else {
                updated.append(metadata)
            }
        }

        let nextSequence = batchItems.last.map { $0.sequence + 1 } ?? session.nextSequence
        let continuationAnchor: Data?
        let nextAnchorKey: String?
        if moreComing {
            let key = "\(Self.continuationPrefix)\(sessionId)|\(nextSequence)"
            continuationAnchor = Data(key.utf8)
            nextAnchorKey = key
        } else {
            continuationAnchor = nil
            nextAnchorKey = nil
        }

        guard dbManager.prepareChangeDeliveryBatch(
            sessionId: sessionId,
            endSequence: nextSequence,
            nextAnchorKey: nextAnchorKey,
            moreComing: moreComing
        ) else {
            logger.error("Could not prepare change delivery batch.")
            return ([], [], false, nil, nil)
        }

        logger.debug("Prepared change delivery batch.")
        return (
            updated,
            deleted,
            moreComing,
            continuationAnchor,
            session.finalAnchorRawValue
        )
    }

    /// Commit the prepared batch after `finishEnumeratingChanges` returns.
    func acknowledgeBatch(deletedOcIds: [String]) {
        lock.lock()
        defer { lock.unlock() }

        guard let sessionId else {
            return
        }

        guard dbManager.acknowledgeChangeDeliveryBatch(sessionId: sessionId, deletedOcIds: deletedOcIds) else {
            logger.error("Could not acknowledge change delivery batch.")
            return
        }

        if dbManager.changeDeliverySession(sessionId: sessionId) == nil {
            self.sessionId = nil
        }
    }

    /// Discard the active durable session when a fresh enumeration replaces an abandoned drain.
    func reset() {
        lock.lock()
        defer { lock.unlock() }

        dbManager.removeChangeDeliverySessions(containerKey: containerKey)
        sessionId = nil
        logger.info("Reset change delivery session.")
    }
}
