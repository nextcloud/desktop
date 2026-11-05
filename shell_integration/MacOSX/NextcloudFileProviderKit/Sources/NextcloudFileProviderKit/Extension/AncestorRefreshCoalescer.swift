//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import os

/// Batches the ocIds of files whose materialization state changed and nudges each of their ancestor containers once per window.
final class AncestorRefreshCoalescer: @unchecked Sendable {
    private static let sharedByDomain = OSAllocatedUnfairLock(initialState: [NSFileProviderDomainIdentifier: AncestorRefreshCoalescer]())

    /// The instance a domain's call sites accumulate into, so a drain never nudges through another domain's manager.
    static func shared(for domain: NSFileProviderDomainIdentifier) -> AncestorRefreshCoalescer {
        sharedByDomain.withLock { coalescers in
            if let coalescer = coalescers[domain] {
                return coalescer
            }

            let coalescer = AncestorRefreshCoalescer()
            coalescers[domain] = coalescer

            return coalescer
        }
    }

    /// Long enough to collapse a folder's sibling files into one pass, short enough to stay imperceptible in the context menu.
    static let defaultWindowNanoseconds: UInt64 = 400_000_000

    private let waitForWindow: @Sendable () async -> Void
    private let lock = NSLock()
    private var pendingOcIds = Set<String>()
    private var drainTask: Task<Void, Never>?

    /// Bumped by every new drain and by ``cancel()``, so a drain holding an older value owns nothing.
    private var drainGeneration: UInt64 = 0

    init(waitForWindow: @escaping @Sendable () async -> Void = { try? await Task.sleep(nanoseconds: AncestorRefreshCoalescer.defaultWindowNanoseconds) }) {
        self.waitForWindow = waitForWindow
    }

    /// Accumulate the given ocIds and, unless a drain is already running, start one with the closures given here.
    func enqueue(
        ocIds: Set<String>,
        ancestors: @escaping @Sendable (Set<String>) -> Set<NSFileProviderItemIdentifier>,
        nudge: @escaping @Sendable (NSFileProviderItemIdentifier) async throws -> Void,
        log: any FileProviderLogging
    ) {
        lock.lock()
        defer { lock.unlock() }

        pendingOcIds.formUnion(ocIds)

        guard drainTask == nil else { return }

        drainGeneration &+= 1
        let generation = drainGeneration
        let logger = FileProviderLogger(category: "AncestorRefreshCoalescer", log: log)
        drainTask = Task {
            await self.drain(generation: generation, ancestors: ancestors, nudge: nudge, logger: logger)
        }
    }

    /// Stop the drain after the nudge it may have in flight, because its closures belong to the extension instance being invalidated, and leave the pending ocIds to the next drain.
    func cancel() {
        lock.lock()
        drainGeneration &+= 1
        let task = drainTask
        drainTask = nil
        lock.unlock()

        task?.cancel()
    }

    private func drain(
        generation: UInt64,
        ancestors: @Sendable (Set<String>) -> Set<NSFileProviderItemIdentifier>,
        nudge: @Sendable (NSFileProviderItemIdentifier) async throws -> Void,
        logger: FileProviderLogger
    ) async {
        var claimed = Set<String>()

        repeat {
            await waitForWindow()
            claimed = claimBatch(generation: generation)

            for container in ancestors(claimed) where owns(generation: generation) {
                do {
                    try await nudge(container)
                    logger.debug("Nudged ancestor container to update Remove download visibility.", [.item: container])
                } catch {
                    logger.error("Could not nudge ancestor container to refresh Remove download visibility.", [.item: container, .error: error.localizedDescription])
                }
            }
        } while !endDrainIfIdle(generation: generation, claimed: claimed)
    }

    private func owns(generation: UInt64) -> Bool {
        lock.lock()
        defer { lock.unlock() }

        return drainGeneration == generation
    }

    private func claimBatch(generation: UInt64) -> Set<String> {
        lock.lock()
        defer { lock.unlock() }

        guard drainGeneration == generation else { return [] }

        let claimed = pendingOcIds
        pendingOcIds.removeAll()

        return claimed
    }

    /// Ends the drain once it no longer owns the coalescer, handing its claimed batch back to the next drain, or once nothing arrived while it was nudging, so no work is left stranded.
    private func endDrainIfIdle(generation: UInt64, claimed: Set<String>) -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard drainGeneration == generation else {
            pendingOcIds.formUnion(claimed)
            return true
        }
        guard pendingOcIds.isEmpty else { return false }

        drainTask = nil

        return true
    }
}
