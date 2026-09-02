//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// Which streaming sessions still have a producer behind them, shared across every ``Enumerator`` in
/// the process.
///
/// This is the correction to the first streaming attempt, which kept "a producer is running" on the
/// buffer instance. The framework routinely serves an intermediate batch from a **new** `Enumerator`
/// — the reason the session is persisted at all — and that instance saw no producer, reported
/// `moreComing: false`, and both ended the sequence and deleted the session out from under the live
/// scan. Since `moreComing: false` tells the framework it is synced, nothing was ever reported
/// again.
///
/// Process-wide is the right scope: the framework replaces enumerators, not the extension process.
/// Deliberately not persisted either — a *new process* means any producer from the old one is gone,
/// and a session with no entry here is treated as abandoned, drained, and finished on its incoming
/// anchor so the next signal re-derives. The deadline covers the remaining case of a producer that
/// hangs rather than exits; expiring one early costs a redundant re-derivation, never a lost change.
///
final class StreamingProducerRegistry: @unchecked Sendable {
    static let shared = StreamingProducerRegistry()

    /// How long a producer stays trusted after its last sign of life.
    static let livenessTimeout: TimeInterval = 10 * 60

    private let lock = NSLock()
    private var deadlines = [String: Date]()

    /// Mark a producer alive, or extend it. Called when a stream opens and on every append.
    func refresh(token: String, now: Date = Date()) {
        lock.lock()
        defer { lock.unlock() }
        deadlines[token] = now.addingTimeInterval(Self.livenessTimeout)
    }

    /// Whether a producer is still expected to append to this session.
    func isLive(token: String, now: Date = Date()) -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard let deadline = deadlines[token] else { return false }

        if now >= deadline {
            deadlines.removeValue(forKey: token)
            return false
        }

        return true
    }

    /// The producer has finished or been superseded.
    func retire(token: String) {
        lock.lock()
        defer { lock.unlock() }
        deadlines.removeValue(forKey: token)
    }

    /// Test seam: pretend a producer stopped signalling long ago.
    func expire(token: String) {
        lock.lock()
        defer { lock.unlock() }
        deadlines[token] = Date(timeIntervalSince1970: 0)
    }
}

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
    /// Open a session for a producer to append to, and register it as live.
    ///
    /// Primed `incomplete` on purpose: the producer's outcome is unknown, so until
    /// ``finishStreaming(token:finalAnchorRawValue:incomplete:)`` says otherwise the session must not
    /// advance the working-set sync point.
    ///
    /// - Returns: A token identifying the stream. Every later call carries it, so a producer whose
    ///   session was replaced under it cannot write to, or finish, the session that replaced it.
    ///
    func primeStreaming(key: String, incomingAnchorRawValue: Data) -> String? {
        prime(key: key, finalAnchorRawValue: incomingAnchorRawValue, updated: [], deleted: [], incomplete: true)

        lock.lock()
        let token = sessionId
        lock.unlock()

        if let token {
            StreamingProducerRegistry.shared.refresh(token: token)
        }

        return token
    }

    /// Queue more changes behind everything stored, and renew the producer's liveness.
    func append(token: String, updated: [SendableItemMetadata], deleted: [SendableItemMetadata]) {
        guard StreamingProducerRegistry.shared.isLive(token: token) else { return }
        guard dbManager.changeDeliverySession(sessionId: token) != nil else { return }

        StreamingProducerRegistry.shared.refresh(token: token)
        _ = dbManager.appendChangeDeliveryItems(sessionId: token, updated: updated, deleted: deleted)
    }

    ///
    /// Retire the producer and record the scan's verdict.
    ///
    /// `incomplete: true` keeps the incoming anchor so the next signal re-derives what this pass
    /// could not read.
    ///
    func finishStreaming(token: String, finalAnchorRawValue: Data, incomplete: Bool) {
        guard StreamingProducerRegistry.shared.isLive(token: token) else { return }

        StreamingProducerRegistry.shared.retire(token: token)

        dbManager.finalizeChangeDeliverySession(
            sessionId: token,
            finalAnchorRawValue: finalAnchorRawValue,
            incomplete: incomplete
        )
    }

    ///
    /// Suspend until this session holds a deliverable item, or its producer is gone.
    ///
    /// Without this a streaming drain would answer `moreComing: true` with an empty batch, and the
    /// framework would call straight back — a spin. Waiting parks the call exactly as the
    /// non-streaming path already parked it for the whole scan, released now at the first wave.
    /// Polled rather than condition-signalled so the wait is a real `await`: blocking a cooperative
    /// thread would starve the scan it is waiting on.
    ///
    func awaitDeliverableItems() async {
        while true {
            guard let token = activeSessionId(), StreamingProducerRegistry.shared.isLive(token: token) else { return }
            guard let session = dbManager.changeDeliverySession(sessionId: token) else { return }

            let hasItems = !dbManager.changeDeliveryItems(
                sessionId: token, fromSequence: session.nextSequence, limit: 1
            ).isEmpty

            if hasItems {
                return
            }

            try? await Task.sleep(nanoseconds: 50_000_000)
        }
    }

    /// The active session identifier. Separate helper because `NSLock` may not be taken from an
    /// async context.
    private func activeSessionId() -> String? {
        lock.lock()
        defer { lock.unlock() }
        return sessionId
    }

    /// Test seam: make this session's producer look abandoned.
    func expireProducerLivenessForTesting(token: String) {
        StreamingProducerRegistry.shared.expire(token: token)
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
        // A live producer means more is coming even when this batch emptied the store. Ending the
        // sequence here would tell the framework it is synced and drop everything the scan has yet
        // to find — and the durable session, which any enumerator may pick up, is what makes this
        // answer the same regardless of which one is serving the batch.
        let moreComing = storedItems.count > budget || StreamingProducerRegistry.shared.isLive(token: sessionId)
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
        StreamingProducerRegistry.shared.retire(token: sessionId)
        self.sessionId = nil
        logger.info("Reset change delivery session.")
    }
}
