//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// Durable FIFO state for a multi-batch File Provider change enumeration.
///
/// The framework can invalidate an enumerator after an intermediate batch and invoke the next batch on a
/// new enumerator. The pending changes and their position are therefore stored in the database. An intermediate
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

    ///
    /// Whether an active durable session is positioned at the given anchor.
    ///
    /// The framework asks for a continuation anchor only after it accepted the batch which ended there, so a batch which was reported up to `key` but not acknowledged yet is acknowledged here.
    ///
    /// Throws when the stored session could not be read. The caller must then finish the enumeration with an error instead of deriving the changes anew, which would discard the stored ones.
    ///
    func isPrimed(forKey key: String) throws -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard let session = try dbManager.changeDeliverySession(forContinuationKey: key, containerKey: containerKey) else {
            sessionId = nil
            return false
        }

        if session.currentAnchorKey != key {
            guard dbManager.acknowledgeChangeDeliveryBatch(sessionId: session.sessionId, endSequence: session.pendingEndSequence) else {
                logger.error("Could not acknowledge the previously reported change delivery batch.")
                return false
            }
        }

        sessionId = session.sessionId
        return true
    }

    /// Whether `key` is an active durable continuation anchor; throws like ``isPrimed(forKey:)``, and only for a continuation key.
    func isContinuation(forKey key: String) throws -> Bool {
        guard key.hasPrefix(Self.continuationPrefix) else {
            return false
        }

        return try isPrimed(forKey: key)
    }

    /// Whether the currently active derivation was incomplete and must retain its incoming sync anchor; throws like ``isPrimed(forKey:)``.
    func isPrimedIncomplete() throws -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard let sessionId else {
            return false
        }

        return try dbManager.changeDeliverySession(sessionId: sessionId)?.incomplete ?? false
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
    /// Returns a continuation anchor for intermediate batches and the session's final anchor for the last, and the identity of the batch to pass to ``acknowledgeBatch(_:)``.
    ///
    /// Returns `nil` when the stored session or its items could not be read, and when the session could not be persisted when the changes were derived. The stored session, if any, is left as it is, so the batch can be prepared again; finishing on an empty batch instead would acknowledge changes which were never delivered.
    ///
    func prepareChangeDeliveryBatch(
        maxItems: Int
    ) -> (
        updated: [SendableItemMetadata],
        deleted: [SendableItemMetadata],
        moreComing: Bool,
        continuationAnchorRawValue: Data?,
        finalAnchorRawValue: Data?,
        acknowledgement: (sessionId: String, endSequence: Int)?
    )? {
        lock.lock()
        defer { lock.unlock() }

        guard let sessionId else {
            return nil
        }

        let session: FilesDatabaseManager.ChangeDeliverySessionState?
        let batch: (items: [(metadataData: Data, deleted: Bool)], endSequence: Int, nextAnchorKey: String?, moreComing: Bool)?
        do {
            session = try dbManager.changeDeliverySession(sessionId: sessionId)
            batch = try dbManager.prepareChangeDeliveryBatch(
                sessionId: sessionId,
                maxItems: max(1, maxItems),
                continuationAnchorKey: { "\(Self.continuationPrefix)\(sessionId)|\($0)" }
            )
        } catch {
            logger.error("Could not prepare the next change delivery batch; the session is kept for a retry.", [.error: error])
            return nil
        }

        // Another enumerator instance finished the session meanwhile.
        guard let session, let batch else {
            return ([], [], false, nil, nil, nil)
        }

        let decoder = JSONDecoder()
        var updated = [SendableItemMetadata]()
        var deleted = [SendableItemMetadata]()

        for item in batch.items {
            guard let metadata = try? decoder.decode(SendableItemMetadata.self, from: item.metadataData) else {
                // The batch is not delivered, so do not leave it reported.
                logger.error("Could not decode change delivery item.")
                dbManager.removeChangeDeliverySessions(containerKey: containerKey)
                self.sessionId = nil
                return ([], [], false, nil, session.finalAnchorRawValue, nil)
            }

            if item.deleted {
                deleted.append(metadata)
            } else {
                updated.append(metadata)
            }
        }

        logger.debug("Prepared change delivery batch.")
        return (
            updated,
            deleted,
            batch.moreComing,
            batch.nextAnchorKey.map { Data($0.utf8) },
            session.finalAnchorRawValue,
            (sessionId, batch.endSequence)
        )
    }

    ///
    /// Commit a batch prepared by ``prepareChangeDeliveryBatch(maxItems:)`` after `finishEnumeratingChanges` returns. Does nothing for `nil`, which stands for a batch that was not prepared.
    ///
    func acknowledgeBatch(_ batch: (sessionId: String, endSequence: Int)?) {
        guard let batch else {
            return
        }

        lock.lock()
        defer { lock.unlock() }

        guard dbManager.acknowledgeChangeDeliveryBatch(sessionId: batch.sessionId, endSequence: batch.endSequence) else {
            logger.error("Could not acknowledge change delivery batch.")
            return
        }

        do {
            if sessionId == batch.sessionId, try dbManager.changeDeliverySession(sessionId: batch.sessionId) == nil {
                sessionId = nil
            }
        } catch {
            logger.error("Could not read the change delivery session after the acknowledgement; it is kept.", [.error: error])
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
