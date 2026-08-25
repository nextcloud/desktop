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
    private var sessionId: String?

    init(dbManager: FilesDatabaseManager, log: any FileProviderLogging) {
        self.dbManager = dbManager
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

            return session.currentAnchorKey == key
        }

        guard key.hasPrefix(Self.continuationPrefix),
              let session = dbManager.changeDeliverySession(forAnchorKey: key)
        else {
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

        if let sessionId {
            dbManager.removeChangeDeliverySession(sessionId: sessionId)
        }

        let newSessionId = UUID().uuidString
        guard dbManager.createChangeDeliverySession(
            sessionId: newSessionId,
            anchorKey: key,
            finalAnchorRawValue: finalAnchorRawValue,
            updated: updated,
            deleted: deleted,
            incomplete: incomplete
        ) else {
            sessionId = nil
            logger.error("Could not persist change delivery session.")
            return
        }

        sessionId = newSessionId
        logger.debug("Persisted change delivery session.")
    }

    ///
    /// Consume the next combined update/delete batch and persist the cursor before returning it.
    ///
    /// `moreComing` is false when this batch consumed the final stored item. Intermediate batches return a
    /// durable continuation anchor; the final batch returns the sync anchor captured when the session was
    /// primed.
    ///
    func takeBatch(
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

        dbManager.advanceChangeDeliverySession(
            sessionId: sessionId,
            nextSequence: nextSequence,
            nextAnchorKey: nextAnchorKey,
            completed: !moreComing
        )

        logger.debug("Consumed change delivery batch.")
        return (
            updated,
            deleted,
            moreComing,
            continuationAnchor,
            session.finalAnchorRawValue
        )
    }

    /// Discard the active durable session when a fresh enumeration replaces an abandoned drain.
    func reset() {
        lock.lock()
        defer { lock.unlock() }

        guard let sessionId else {
            return
        }

        dbManager.removeChangeDeliverySession(sessionId: sessionId)
        self.sessionId = nil
        logger.info("Reset change delivery session.")
    }
}
