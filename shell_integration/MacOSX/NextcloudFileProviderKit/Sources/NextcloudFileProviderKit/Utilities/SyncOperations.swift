//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

/// Tracks requests using one database manager. A request can start a server write only while
/// admission is open. Account setup cancels the other requests and waits for writes to finish.
final class SyncOperations: @unchecked Sendable {
    private enum State {
        case accepting
        case draining
        case invalidated
    }

    private enum RequestState {
        case reading
        case writing
        case delivering
        case stopped
    }

    private struct Request {
        var state = RequestState.reading
        var cancelRead: [@Sendable () -> Void]
        var resumeWrite: (@Sendable () -> Void)?
    }

    final class Operation: Sendable {
        @TaskLocal static var current: Operation?

        private let operations: SyncOperations
        private let id: UUID

        fileprivate init(operations: SyncOperations, id: UUID) {
            self.operations = operations
            self.id = id
        }

        var isStopped: Bool { operations.isStopped(id) }

        /// Claim the write before sending it, so closing admission and sending a mutation
        /// cannot pass each other. Keep the claim until the result and any follow-up work finish.
        func beginServerWrite() -> Bool {
            operations.claim(id, as: .writing)
        }

        /// Once network work is done, finish the local result before setup can publish a new account.
        func beginDelivery() -> Bool {
            operations.claim(id, as: .delivering)
        }

        func cancelReadOnStop(_ action: @escaping @Sendable () -> Void) {
            operations.cancelReadOnStop(id, action: action)
        }

        func registerReadTask(_ task: URLSessionTask) {
            cancelReadOnStop { task.cancel() }
        }

        func run(_ work: sending @escaping () async -> Void) {
            let task = Task {
                defer { finish() }
                await Self.$current.withValue(self, operation: work)
            }
            cancelReadOnStop { task.cancel() }
        }

        func finish() { operations.finish(id) }

        deinit { finish() }
    }

    private let lock = NSLock()
    private let requestsFinishing = DispatchGroup()
    private var state = State.accepting
    private var requests: [UUID: Request] = [:]

    var isAcceptingWork: Bool {
        lock.withLock { state == .accepting }
    }

    var isInvalidated: Bool {
        lock.withLock { state == .invalidated }
    }

    func begin(
        onStop: (@Sendable () -> Void)? = nil,
        resumeWrite: (@Sendable () -> Void)? = nil
    ) -> Operation? {
        lock.withLock {
            guard state == .accepting else { return nil }
            let id = UUID()
            requests[id] = Request(cancelRead: onStop.map { [$0] } ?? [], resumeWrite: resumeWrite)
            return Operation(operations: self, id: id)
        }
    }

    /// Item APIs can also be called without a File Provider request.
    static func beginServerWrite() -> Bool {
        Operation.current?.beginServerWrite() ?? true
    }

    private func claim(_ id: UUID, as requestState: RequestState) -> Bool {
        lock.withLock {
            guard state != .invalidated else { return false }
            switch requests[id]?.state {
                case .writing:
                    return true
                case .delivering:
                    return requestState == .delivering
                case .reading where state == .accepting:
                    requestsFinishing.enter()
                    requests[id]?.state = requestState
                    requests[id]?.cancelRead.removeAll()
                    return true
                default:
                    return false
            }
        }
    }

    private func isStopped(_ id: UUID) -> Bool {
        lock.withLock {
            state == .invalidated || requests[id] == nil || requests[id]?.state == .stopped
        }
    }

    private func cancelReadOnStop(_ id: UUID, action: @escaping @Sendable () -> Void) {
        let cancelNow = lock.withLock {
            if state == .invalidated { return true }
            switch requests[id]?.state {
                case .reading:
                    requests[id]?.cancelRead.append(action)
                    return false
                case .writing, .delivering:
                    return false
                default:
                    return true
            }
        }
        if cancelNow { action() }
    }

    private func finish(_ id: UUID) {
        let request = lock.withLock { requests.removeValue(forKey: id) }
        if request?.state == .writing || request?.state == .delivering { requestsFinishing.leave() }
    }

    func stopAcceptingWork() {
        lock.withLock {
            guard state == .accepting else { return }
            state = .draining
            for id in requests.keys where requests[id]?.state == .reading {
                requests[id]?.state = .stopped
            }
        }
    }

    /// Reopen admission after a failed replacement leaves this account and its database usable.
    func resumeAcceptingWork() -> Bool {
        lock.withLock {
            guard state == .draining else { return false }
            state = .accepting
            return true
        }
    }

    func invalidate() {
        lock.withLock { state = .invalidated }
    }

    /// Cancel reads, resume paused uploads, and wait for writes and results already being delivered.
    func waitForWrites() async {
        let actions = lock.withLock {
            var callbacks: [@Sendable () -> Void] = []
            for id in requests.keys {
                if requests[id]?.state == .writing, let resume = requests[id]?.resumeWrite {
                    callbacks.append(resume)
                    requests[id]?.resumeWrite = nil
                }
                if let cancel = requests[id]?.cancelRead, !cancel.isEmpty {
                    callbacks.append(contentsOf: cancel)
                    requests[id]?.cancelRead.removeAll()
                }
            }
            return callbacks
        }
        for action in actions { action() }
        await withCheckedContinuation { continuation in
            requestsFinishing.notify(queue: .global()) { continuation.resume() }
        }
    }
}
