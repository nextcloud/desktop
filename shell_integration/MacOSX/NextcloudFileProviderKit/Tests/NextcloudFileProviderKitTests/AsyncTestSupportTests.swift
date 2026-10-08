// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import Testing

@Suite(.timeLimit(.minutes(1)))
struct AsyncTestSupportTests {
    @Test
    func streamDeliversValuesAndFinishes() async throws {
        let (stream, continuation) = AsyncStream<Int>.makeStream()
        continuation.yield(42)
        continuation.finish()

        #expect(try await nextTestValue(from: stream) == 42)
        #expect(try await nextTestValue(from: stream) == nil)
    }

    @Test
    func missingStreamValueTimesOut() async {
        let (stream, continuation) = AsyncStream<Int>.makeStream()
        defer { continuation.finish() }
        do {
            _ = try await nextTestValue(from: stream, timeout: .zero)
            Issue.record("A missing value must time out")
        } catch {
            #expect((error as? URLError)?.code == .timedOut)
        }
    }

    @Test
    func cancelledStreamWaitThrowsCancellation() async throws {
        let (stream, continuation) = AsyncStream<Int>.makeStream()
        defer { continuation.finish() }
        let task = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            do {
                _ = try await nextTestValue(from: stream)
                Issue.record("A cancelled wait must throw")
            } catch {
                #expect(error is CancellationError)
            }
        }
        try await testTaskValue(of: task)
    }

    @Test
    func taskCanReturnNil() async throws {
        let task = Task<Int?, Never> { nil }
        #expect(try await testTaskValue(of: task) == nil)
    }

    @Test
    func noncooperativeTaskTimesOutAndCanBeReleased() async throws {
        let (started, continuation) = AsyncStream<CheckedContinuation<Int, Never>>.makeStream()
        defer { continuation.finish() }
        let task = Task {
            await withCheckedContinuation { continuation.yield($0) }
        }
        let release = try #require(try await nextTestValue(from: started))
        var released = false
        defer {
            if !released {
                release.resume(returning: 42)
            }
            task.cancel()
        }

        do {
            _ = try await testTaskValue(of: task, timeout: .zero)
            Issue.record("A task ignoring cancellation must still time out")
        } catch {
            #expect((error as? URLError)?.code == .timedOut)
        }
        #expect(task.isCancelled)
        release.resume(returning: 42)
        released = true
        #expect(try await testTaskValue(of: task) == 42)
    }

    @Test
    func cancellationWaitReportsTimeout() async {
        #expect(await waitForCancellation(timeout: .zero) == false)
    }

    @Test
    func cancellationWaitReportsCancellation() async throws {
        let task = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            return await waitForCancellation()
        }
        #expect(try await testTaskValue(of: task))
    }
}
