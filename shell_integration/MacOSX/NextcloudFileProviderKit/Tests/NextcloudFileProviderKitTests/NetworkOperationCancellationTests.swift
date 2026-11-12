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
    func accountChangeResumesAnUploadAndPreventsAnotherPause() async throws {
        let (suspended, notifySuspended) = AsyncStream<Void>.makeStream()
        let (resumed, notifyResumed) = AsyncStream<Void>.makeStream()
        let monitor = ClosureEventMonitor()
        monitor.requestDidSuspend = { _ in notifySuspended.yield(()) }
        monitor.requestDidResume = { _ in notifyResumed.yield(()) }
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MockHTTPURLProtocol.self]
        let host = UUID().uuidString.lowercased() + ".example.invalid"
        MockHTTPURLProtocol.setHandlers(forHost: host, start: { _ in }, stop: { _ in })
        defer { MockHTTPURLProtocol.removeHandlers(forHost: host) }
        let session = Session(
            configuration: configuration,
            startRequestsImmediately: false,
            eventMonitors: [monitor]
        )
        let request = session.upload(Data(), to: "https://" + host + "/file")
        defer { request.cancel() }
        let progress = Progress()
        let operations = SyncOperations()
        let pendingOperation = operations.begin(resumeWrite: { progress.resume() })
        let operation = try #require(pendingOperation)
        try #require(operation.beginServerWrite())
        defer { operation.finish() }

        let resumedDuringChange = await NetworkOperationCancellation(log: FileProviderLogMock()).run(progress: progress) { cancellation in
            cancellation.register(request: request, canPause: { operations.isAcceptingWork })
            progress.pause()
            guard (try? await nextTestValue(from: suspended)) != nil else { return false }

            operations.stopAcceptingWork()
            let drain = Task { await operations.waitForWrites() }
            let resumedRequest: Void? = try? await nextTestValue(from: resumed)
            progress.pausingHandler?()
            let didResume = resumedRequest != nil && request.isResumed
            operation.finish()
            await drain.value
            return didResume
        }
        #expect(resumedDuringChange)
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
    func stoppedReadCannotSendAWriteWhenAccountSetupResumes() async throws {
        let operations = SyncOperations()
        let operation = try #require(operations.begin())
        defer { operation.finish() }

        operations.stopAcceptingWork()
        await operations.waitForWrites()
        #expect(operations.resumeAcceptingWork())

        #expect(operation.isStopped)
        #expect(!operation.beginServerWrite())
        #expect(!operation.beginDelivery())
        let newOperation = try #require(operations.begin())
        defer { newOperation.finish() }
        #expect(newOperation.beginServerWrite())
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
