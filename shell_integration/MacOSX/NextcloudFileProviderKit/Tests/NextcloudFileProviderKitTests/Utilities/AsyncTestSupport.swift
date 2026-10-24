// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

let asyncTestTimeout: Duration = .seconds(5)

/// Returns false if cancellation never reaches the operation before the test timeout.
func waitForCancellation(timeout: Duration = asyncTestTimeout) async -> Bool {
    do {
        try await Task.sleep(for: timeout)
        return false
    } catch {
        return Task.isCancelled
    }
}

/// Bounds a stream wait; cancelling its iterator releases the losing task.
func nextTestValue<Value: Sendable>(
    from stream: AsyncStream<Value>,
    timeout: Duration = asyncTestTimeout
) async throws -> Value? {
    try Task.checkCancellation()
    let value = try await withThrowingTaskGroup(of: Value?.self) { group in
        group.addTask {
            var iterator = stream.makeAsyncIterator()
            return await iterator.next()
        }
        group.addTask {
            try await Task.sleep(for: timeout)
            throw URLError(.timedOut)
        }
        defer { group.cancelAll() }
        guard let value = try await group.next() else {
            throw CancellationError()
        }
        return value
    }
    try Task.checkCancellation()
    return value
}

/// Bounds waiting for a task even when it ignores cancellation.
/// A separate waiter keeps `task.value` out of the timeout's task group.
func testTaskValue<Value: Sendable>(
    of task: Task<Value, Never>,
    timeout: Duration = asyncTestTimeout
) async throws -> Value {
    let (results, continuation) = AsyncStream<Value>.makeStream()
    let waiter = Task {
        let value = await task.value
        continuation.yield(value)
        continuation.finish()
    }
    defer {
        waiter.cancel()
        continuation.finish()
    }
    do {
        guard let value = try await nextTestValue(from: results, timeout: timeout) else {
            throw CancellationError()
        }
        return value
    } catch {
        task.cancel()
        throw error
    }
}
