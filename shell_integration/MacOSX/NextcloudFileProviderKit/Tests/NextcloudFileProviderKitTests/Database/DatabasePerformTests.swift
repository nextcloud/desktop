//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    /// Guards for the properties of ``FilesDatabaseManager/perform(_:)`` that keep concurrent enumeration from starving the cooperative pool.
    @Suite("Perform")
    struct DatabasePerformTests {
        let manager = DatabaseTestSuites.makeManager()

        /// The work must leave the caller's executor, asserted by asking where the body ran rather than by timing it.
        @Test func workRunsOffTheCallingExecutorAndOnTheBlockingCallQueue() async {
            let onBlockingCallQueue = await manager.perform { $0.isOnBlockingCallQueue }

            #expect(onBlockingCallQueue, "Database work must run on the blocking-call queue, not on the cooperative pool thread that asked for it.")
            #expect(!manager.isOnBlockingCallQueue, "The caller itself must not be on the blocking-call queue; that would mean the hop did not happen.")
        }

        /// Each body signals and then waits for the other, so a queue that ran them one after the other makes whichever runs first time out, in either start order.
        @Test func concurrentCallersAreNotSerialised() async {
            let manager = manager
            let firstStarted = DispatchSemaphore(value: 0)
            let secondStarted = DispatchSemaphore(value: 0)

            async let firstSawSecond = manager.perform { _ in
                firstStarted.signal()
                return secondStarted.wait(timeout: .now() + 5) == .success
            }
            async let secondSawFirst = manager.perform { _ in
                secondStarted.signal()
                return firstStarted.wait(timeout: .now() + 5) == .success
            }

            let (first, second) = await (firstSawSecond, secondSawFirst)

            #expect(first && second, "Callers must not be serialised by the hop; the pool already serialises writes and runs reads in parallel.")
        }

        /// Concurrent hops rely on the pool to serialise writes, so none of them may be lost.
        @Test func concurrentWritesAreAllPersisted() async {
            let manager = manager
            let ocIds = (0 ..< 64).map { "concurrent-\($0)" }

            await withTaskGroup(of: Void.self) { group in
                for ocId in ocIds {
                    group.addTask {
                        await manager.perform { manager in
                            manager.addItemMetadata(DatabaseTestSuites.makeFile(ocId: ocId, fileName: "\(ocId).txt"))
                        }
                    }
                }
            }

            let stored = await manager.perform { manager in
                ocIds.filter { manager.itemMetadata(ocId: $0) != nil }.count
            }

            #expect(stored == ocIds.count, "Every concurrent write must be persisted.")
        }
    }
}
