// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
import Foundation

/// Bridges Progress cancellation to the Swift task and registered network requests,
/// since cancelling the task alone does not stop requests managed through callbacks.
final class NetworkOperationCancellation: @unchecked Sendable {
    private let logger: FileProviderLogger
    private let lock = NSLock()
    private var cancelled = false
    private var finished = false
    private var progress: Progress?
    private var taskCancellation: (@Sendable () -> Void)?
    private var request: Request?
    private var sessionTask: URLSessionTask?

    init(log: any FileProviderLogging) {
        logger = FileProviderLogger(category: "NetworkOperationCancellation", log: log)
    }

    var isCancelled: Bool {
        lock.lock()
        defer { lock.unlock() }
        return cancelled
    }

    func cancel() {
        lock.lock()
        let shouldLog = !cancelled && !finished
        cancelled = true
        let taskCancellation = taskCancellation
        let request = request
        let sessionTask = sessionTask
        lock.unlock()
        if shouldLog {
            logger.debug("Cancelling the network operation.")
        }
        taskCancellation?()
        request?.cancel()
        sessionTask?.cancel()
    }

    func register(request: Request) {
        lock.lock()
        if !finished {
            self.request = request
            progress?.pausingHandler = { request.suspend() }
            progress?.resumingHandler = { request.resume() }
        }
        let cancelled = cancelled || finished
        lock.unlock()
        if cancelled {
            request.cancel()
        }
    }

    func register(task: URLSessionTask) {
        lock.lock()
        if !finished {
            sessionTask = task
        }
        let cancelled = cancelled || finished
        lock.unlock()
        if cancelled {
            task.cancel()
        }
    }

    func run<Result: Sendable>(
        progress: Progress,
        operation: sending @escaping (NetworkOperationCancellation) async -> Result
    ) async -> Result {
        lock.withLock {
            self.progress = progress
            progress.cancellationHandler = { self.cancel() }
        }
        if progress.isCancelled || Task.isCancelled {
            cancel()
        }
        let task = Task { await operation(self) }
        registerTaskCancellation { task.cancel() }
        if progress.isCancelled {
            cancel()
        }
        defer { finish() }
        return await withTaskCancellationHandler {
            await task.value
        } onCancel: {
            self.cancel()
        }
    }

    private func registerTaskCancellation(_ handler: @Sendable @escaping () -> Void) {
        lock.lock()
        taskCancellation = handler
        let cancelled = cancelled
        lock.unlock()
        if cancelled {
            handler()
        }
    }

    private func finish() {
        lock.lock()
        finished = true
        progress?.cancellationHandler = nil
        progress?.pausingHandler = nil
        progress?.resumingHandler = nil
        progress = nil
        taskCancellation = nil
        request = nil
        sessionTask = nil
        lock.unlock()
    }
}
