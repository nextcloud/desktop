// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing

@Suite(.timeLimit(.minutes(1)))
struct NetworkOperationCancellationTests {
    @Test
    func progressCancellationCancelsRunningTask() async {
        let progress = Progress()
        let cancelled = await NetworkOperationCancellation(log: FileProviderLogMock()).run(progress: progress) { _ in
            progress.cancel()
            let cancelled = await waitForCancellation()
            #expect(cancelled)
            return Task.isCancelled
        }
        #expect(cancelled)
        #expect(progress.cancellationHandler == nil)
    }

    @Test
    func parentCancellationReachesOperationTask() async throws {
        let progress = Progress()
        let (started, continuation) = AsyncStream<Void>.makeStream()
        defer { continuation.finish() }
        let task = Task {
            await NetworkOperationCancellation(log: FileProviderLogMock()).run(progress: progress) { _ in
                continuation.yield(())
                let cancelled = await waitForCancellation()
                #expect(cancelled)
                return Task.isCancelled
            }
        }
        defer { task.cancel() }
        try #require(try await nextTestValue(from: started) != nil)
        task.cancel()
        #expect(try await testTaskValue(of: task))
        #expect(progress.cancellationHandler == nil)
    }

    @Test(arguments: [false, true])
    func networkTaskIsCancelledEvenWhenRegisteredLate(cancelBeforeRegistration: Bool) async throws {
        let cancellation = NetworkOperationCancellation(log: FileProviderLogMock())
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let (completion, continuation) = AsyncStream<Int>.makeStream()
        defer { continuation.finish() }
        let task = try session.dataTask(with: #require(URL(string: "https://example.invalid/resource"))) { _, _, error in
            continuation.yield((error as NSError?)?.code ?? 0)
            continuation.finish()
        }
        if cancelBeforeRegistration {
            cancellation.cancel()
        }
        cancellation.register(task: task)
        if !cancelBeforeRegistration {
            cancellation.cancel()
        }
        #expect(try await nextTestValue(from: completion) == NSURLErrorCancelled)
    }

    @Test
    func networkRequestIsCancelledAndHandlersAreReleased() async {
        let (cancelledRequests, continuation) = AsyncStream<ObjectIdentifier>.makeStream()
        defer { continuation.finish() }
        let monitor = ClosureEventMonitor()
        monitor.requestDidCancel = { continuation.yield(ObjectIdentifier($0)) }
        let session = Session(startRequestsImmediately: false, eventMonitors: [monitor])
        let requests: [Request] = [
            session.upload(Data(), to: "https://example.invalid/resource"),
            session.download("https://example.invalid/resource"),
            session.request("https://example.invalid/resource")
        ]
        for request in requests {
            let progress = Progress()
            let cancelled = await NetworkOperationCancellation(log: FileProviderLogMock()).run(progress: progress) { cancellation in
                cancellation.register(request: request)
                #expect(progress.pausingHandler != nil)
                #expect(progress.resumingHandler != nil)
                progress.cancel()
                let cancelled = await waitForCancellation()
                #expect(cancelled)
                // The worker must outlive cancellation of the registered request.
                let cancelledRequest = await Task.detached {
                    try? await nextTestValue(from: cancelledRequests)
                }.value
                #expect(cancelledRequest == ObjectIdentifier(request))
                #expect(request.isCancelled)
                return Task.isCancelled
            }
            #expect(cancelled)
            #expect(progress.cancellationHandler == nil)
            #expect(progress.pausingHandler == nil)
            #expect(progress.resumingHandler == nil)
        }
    }

    @Test
    func requestRegisteredAfterCancellationIsCancelled() async throws {
        let (cancelledRequests, continuation) = AsyncStream<ObjectIdentifier>.makeStream()
        defer { continuation.finish() }
        let monitor = ClosureEventMonitor()
        monitor.requestDidCancel = { continuation.yield(ObjectIdentifier($0)) }
        let session = Session(startRequestsImmediately: false, eventMonitors: [monitor])
        let requests: [Request] = [
            session.upload(Data(), to: "https://example.invalid/resource"),
            session.download("https://example.invalid/resource"),
            session.request("https://example.invalid/resource")
        ]
        for request in requests {
            let cancellation = NetworkOperationCancellation(log: FileProviderLogMock())
            cancellation.cancel()
            cancellation.register(request: request)
            try #require(request.isCancelled)
            #expect(try await nextTestValue(from: cancelledRequests) == ObjectIdentifier(request))
        }
    }

    @Test
    func taskRegisteredAfterCompletionIsCancelled() async throws {
        let cancellation = NetworkOperationCancellation(log: FileProviderLogMock())
        await cancellation.run(progress: Progress()) { _ in }
        let session = URLSession(configuration: .ephemeral)
        defer { session.invalidateAndCancel() }
        let (completion, continuation) = AsyncStream<Int>.makeStream()
        defer { continuation.finish() }
        let task = try session.dataTask(with: #require(URL(string: "https://example.invalid/resource"))) { _, _, error in
            continuation.yield((error as NSError?)?.code ?? 0)
            continuation.finish()
        }
        cancellation.register(task: task)
        #expect(try await nextTestValue(from: completion) == NSURLErrorCancelled)
    }

    @Test
    func completionReleasesCancellationHandler() async {
        let progress = Progress()
        weak var released: NetworkOperationCancellation?
        do {
            let cancellation = NetworkOperationCancellation(log: FileProviderLogMock())
            released = cancellation
            let result = await cancellation.run(progress: progress) { _ in 42 }
            #expect(result == 42)
        }
        #expect(progress.cancellationHandler == nil)
        #expect(released == nil)
    }
}
