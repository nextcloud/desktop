//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Alamofire
import Foundation
import NextcloudCapabilitiesKit
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
@testable import NextcloudKit
import Testing
@testable import TestInterface

@Suite(.serialized)
struct RemoteInterfaceExtensionTests {
    private func capabilitiesFromMockJSON(jsonString: String = mockCapabilities) -> (Capabilities, Data) {
        let data = jsonString.data(using: .utf8)!
        let caps = Capabilities(data: data)!
        return (caps, data)
    }

    private func makeNetworkTask() throws -> (URLSession, URLSessionTask, AsyncStream<Int>) {
        let url = try #require(URL(string: "https://example.invalid/capabilities"))
        let session = URLSession(configuration: .ephemeral)
        let (task, completion) = observedNetworkTask(in: session, url: url)
        return (session, task, completion)
    }

    /// Each test instance uses its own key in the shared capabilities cache.
    let testAccount = Account(
        user: "a1",
        id: "1",
        serverUrl: "https://\(UUID().uuidString.lowercased()).example.invalid",
        password: "pass"
    )
    let otherAccount = Account(user: "a2", id: "2", serverUrl: "example.com", password: "word")

    @Test(arguments: [false, true])
    func cancellingCapabilitiesCallersPreservesRequestUntilLastCaller(cancelRemainingCaller: Bool) async throws {
        let actor = RetrievedCapabilitiesActor.shared
        let (session, networkTask, completion) = try makeNetworkTask()
        defer { session.invalidateAndCancel() }
        let (capabilities, data) = capabilitiesFromMockJSON()
        let (started, startedContinuation) = AsyncStream<Void>.makeStream()
        let (finish, finishContinuation) = AsyncStream<Void>.makeStream()
        defer {
            startedContinuation.finish()
            finishContinuation.finish()
        }
        try await confirmation("One shared capabilities request", expectedCount: 1) { fetched in
            let remote = TestableRemoteInterface { account, options, taskHandler in
                fetched()
                #expect(account.ncKitAccount == testAccount.ncKitAccount)
                #expect(options.customHeader == nil)
                taskHandler(networkTask)
                startedContinuation.yield(())
                do {
                    guard try await nextTestValue(from: finish) != nil else {
                        return (account.ncKitAccount, nil, nil, .invalidResponseError)
                    }
                } catch {
                    return (account.ncKitAccount, nil, nil, .cancelled)
                }
                #expect(!Task.isCancelled)
                return (account.ncKitAccount, capabilities, data, .success)
            }
            let progress = Progress()
            let first = Task {
                await NetworkOperationCancellation(log: FileProviderLogMock()).run(progress: progress) { _ in
                    await remote.currentCapabilities(account: testAccount)
                }
            }
            defer { first.cancel() }
            try #require(try await nextTestValue(from: started) != nil)
            let secondRemote = TestableRemoteInterface { account, _, _ in
                Issue.record("The remaining caller must share the first request")
                return (account.ncKitAccount, nil, nil, .invalidResponseError)
            }
            let second = Task { await secondRemote.currentCapabilities(account: testAccount) }
            defer { second.cancel() }
            let deadline = ContinuousClock.now + asyncTestTimeout
            while await actor.waitingCallerCount(forAccount: testAccount.ncKitAccount) < 2 {
                guard ContinuousClock.now < deadline else { throw URLError(.timedOut) }
                try await Task.sleep(for: .milliseconds(1))
            }
            progress.cancel()

            let cancelled = try await testTaskValue(of: first)
            #expect(cancelled.error.errorCode == NSURLErrorCancelled)
            #expect(cancelled.capabilities == nil)
            #expect(networkTask.state == .suspended)
            if cancelRemainingCaller {
                second.cancel()
                #expect(try await testTaskValue(of: second).error == .cancelled)
                #expect(try await nextTestValue(from: completion) == NSURLErrorCancelled)
                #expect(await actor.getCapabilities(for: testAccount.ncKitAccount) == nil)
                return
            }
            finishContinuation.yield(())
            let result = try await testTaskValue(of: second)
            #expect(result.error == .success)
            #expect(result.capabilities == capabilities)
            #expect(result.data == data)
            let cached = await remote.currentCapabilities(account: testAccount)
            #expect(cached.error == .success)
            #expect(cached.capabilities == capabilities)
            #expect(cached.data == nil)
        }
    }

    @Test(arguments: [false, true], [false, true])
    func lastCapabilitiesCallerCancellationAllowsReplacement(
        registerAfterCancellation: Bool, finishReplacementFirst: Bool
    ) async throws {
        let actor = RetrievedCapabilitiesActor.shared
        let (session, networkTask, completion) = try makeNetworkTask()
        defer { session.invalidateAndCancel() }
        let (oldCapabilities, oldData) = capabilitiesFromMockJSON()
        let (newCapabilities, newData) = capabilitiesFromMockJSON(
            jsonString: mockCapabilities.replacingOccurrences(of: "\"undelete\": true", with: "\"undelete\": false")
        )
        #expect(oldCapabilities != newCapabilities)
        let (started, startedContinuation) = AsyncStream<Void>.makeStream()
        let (releaseOld, releaseOldContinuation) = AsyncStream<Void>.makeStream()
        let (releaseNew, releaseNewContinuation) = AsyncStream<Void>.makeStream()
        // Separate gates let the backend deliver a late response despite its caller's cancellation.
        let oldGate = Task.detached { try? await nextTestValue(from: releaseOld) }
        let newGate = Task.detached { try? await nextTestValue(from: releaseNew) }
        defer {
            startedContinuation.finish()
            releaseOldContinuation.finish()
            releaseNewContinuation.finish()
            oldGate.cancel()
            newGate.cancel()
        }
        let oldRemote = TestableRemoteInterface { account, _, taskHandler in
            if !registerAfterCancellation {
                taskHandler(networkTask)
            }
            startedContinuation.yield(())
            _ = await oldGate.value
            if registerAfterCancellation {
                taskHandler(networkTask)
            }
            return (account.ncKitAccount, oldCapabilities, oldData, .success)
        }
        let first = Task { await oldRemote.currentCapabilities(account: testAccount) }
        defer { first.cancel() }
        try #require(try await nextTestValue(from: started) != nil)
        let oldWorker = try #require(await actor.sharedFetches[testAccount.ncKitAccount]?.task)
        first.cancel()
        #expect(try await testTaskValue(of: first).error == .cancelled)
        #expect(oldWorker.isCancelled)
        #expect(await actor.sharedFetches[testAccount.ncKitAccount] == nil)
        if !registerAfterCancellation {
            #expect(try await nextTestValue(from: completion) == NSURLErrorCancelled)
        }
        let replacementRemote = TestableRemoteInterface { account, _, _ in
            startedContinuation.yield(())
            _ = await newGate.value
            #expect(!Task.isCancelled)
            return (account.ncKitAccount, newCapabilities, newData, .success)
        }
        let replacement = Task { await replacementRemote.currentCapabilities(account: testAccount) }
        defer { replacement.cancel() }
        try #require(try await nextTestValue(from: started) != nil)
        if finishReplacementFirst {
            releaseNewContinuation.yield(())
            #expect(try await testTaskValue(of: replacement).capabilities == newCapabilities)
        }
        releaseOldContinuation.yield(())
        try await testTaskValue(of: oldWorker)
        if registerAfterCancellation {
            #expect(try await nextTestValue(from: completion) == NSURLErrorCancelled)
        }
        if finishReplacementFirst {
            #expect(await actor.getCapabilities(for: testAccount.ncKitAccount)?.capabilities == newCapabilities)
        } else {
            #expect(await actor.getCapabilities(for: testAccount.ncKitAccount) == nil)
            #expect(await actor.sharedFetches[testAccount.ncKitAccount] != nil)
            releaseNewContinuation.yield(())
            #expect(try await testTaskValue(of: replacement).capabilities == newCapabilities)
        }
        #expect(await actor.sharedFetches[testAccount.ncKitAccount] == nil)
        let cacheOnlyRemote = TestableRemoteInterface { account, _, _ in
            Issue.record("The replacement's successful capabilities must remain cached")
            return (account.ncKitAccount, nil, nil, .invalidResponseError)
        }
        #expect(await cacheOnlyRemote.currentCapabilities(account: testAccount).capabilities == newCapabilities)
    }

    @Test func cancelledCapabilitiesDoNotStartAnotherFetch() async {
        let remote = TestableRemoteInterface { account, _, _ in
            Issue.record("Cancelled capability lookup must not start a request")
            return (account.ncKitAccount, nil, nil, .invalidResponseError)
        }
        let task = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            return await remote.currentCapabilities(account: testAccount)
        }
        let result = await task.value
        #expect(result.error.errorCode == NSURLErrorCancelled)
        #expect(result.capabilities == nil)
    }

    @Test func chunkedUploadRemotePathComponentsPreserveFilenameCharacters() throws {
        let serverUrl = "https://cloud.example.com/remote.php/dav/files/user/comics"
        let fileNames = [
            "The Nightly News #001 (2011).cbz",
            "Question?.txt",
            "Literal%23Name.txt"
        ]

        for fileName in fileNames {
            let components = try #require(chunkedUploadRemotePathComponents(from: "\(serverUrl)/\(fileName)"))

            #expect(components.serverUrl == serverUrl)
            #expect(components.destinationFileName == fileName)
        }
    }

    @Test func chunkedUploadRemotePathComponentsRejectInvalidPaths() {
        #expect(chunkedUploadRemotePathComponents(from: "filename.txt") == nil)
        #expect(chunkedUploadRemotePathComponents(from: "https://cloud.example.com/") == nil)
    }

    @Test func cancelledChunkedUploadPreservesCancellationError() async throws {
        let account = Account(user: UUID().uuidString, id: "user", serverUrl: "https://example.invalid", password: "password")
        let remote = NextcloudKit()
        remote.appendSession(
            account: account.ncKitAccount,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id,
            password: account.password,
            userAgent: "cancellation-test",
            groupIdentifier: ""
        )
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let uploadIdentifier = UUID().uuidString
        let chunksDirectory = FileManager.default.temporaryDirectory.appendingPathComponent(uploadIdentifier)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer {
            try? FileManager.default.removeItem(at: directory)
            try? FileManager.default.removeItem(at: chunksDirectory)
        }
        let contents = directory.appendingPathComponent("video.bin")
        try Data(repeating: 1, count: 8).write(to: contents)
        let task = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            let result = await remote.chunkedUpload(
                localPath: contents.path,
                remotePath: account.davFilesUrl + "/video.bin",
                remoteChunkStoreFolderName: uploadIdentifier,
                chunkSize: 3,
                remainingChunks: [],
                account: account,
                log: FileProviderLogMock()
            )
            #expect(result.file == nil)
            #expect(result.chunksDirectory == chunksDirectory)
            return result.nkError.errorCode
        }
        #expect(await task.value == NSURLErrorCancelled)
    }

    @Test func currentCapabilitiesReturnsFreshCache() async {
        let remoteInterface = TestableRemoteInterface { _, _, _ in
            Issue.record("fetchCapabilities should NOT be called when cache is fresh.")
            return (testAccount.ncKitAccount, nil, nil, .invalidResponseError)
        }

        let (freshCaps, _) = capabilitiesFromMockJSON()
        let freshDate = Date() // Now

        // Setup: Put fresh data into the shared actor
        await RetrievedCapabilitiesActor.shared.setCapabilities(
            forAccount: testAccount.ncKitAccount,
            capabilities: freshCaps,
            retrievedAt: freshDate
        )

        let result = await remoteInterface.currentCapabilities(account: testAccount)

        #expect(result.error == .success)
        #expect(result.capabilities == freshCaps)
        #expect(result.data == nil, "Data should be nil as no fetch occurred")
        #expect(result.account == testAccount.ncKitAccount)
    }

    @Test func currentCapabilitiesFetchesOnNoCache() async {
        let (fetchedCaps, fetchedData) = capabilitiesFromMockJSON()

        await confirmation("fetcherCalled") { fetcherCalled in
            let remoteInterface = TestableRemoteInterface { acc, _, _ in
                fetcherCalled()
                #expect(acc.ncKitAccount == testAccount.ncKitAccount)
                return (acc.ncKitAccount, fetchedCaps, fetchedData, .success)
            }

            let result = await remoteInterface.currentCapabilities(account: testAccount)

            #expect(result.error == .success)
            #expect(result.capabilities == fetchedCaps)
            #expect(result.data == fetchedData)
        }

        let actorCache = await RetrievedCapabilitiesActor.shared.getCapabilities(for: testAccount.ncKitAccount)
        #expect(actorCache?.capabilities == fetchedCaps)
    }

    @Test func currentCapabilitiesFetchesOnStaleCache() async {
        let (staleCaps, _) = capabilitiesFromMockJSON(jsonString: """
        {
            "ocs": {
                "meta": {
                    "status": "ok",
                    "statuscode": 100,
                    "message": "OK"
                },
                "data": {
                    "version": {
                        "major": 31,
                        "minor": 0,
                        "micro": 9,
                        "string": "31.0.9",
                        "edition": "",
                        "extendedSupport": false
                    },
                    "capabilities": {
                        "files": {
                            "undelete": false
                        }
                    }
                }
            }
        }
        """) // Different caps
        let staleDate = Date(timeIntervalSinceNow: -(CapabilitiesFetchInterval + 300)) // Definitely stale

        // Setup: Put stale data into the actor
        await RetrievedCapabilitiesActor.shared.setCapabilities(
            forAccount: testAccount.ncKitAccount,
            capabilities: staleCaps,
            retrievedAt: staleDate
        )

        let (newCaps, newData) = capabilitiesFromMockJSON() // Fresh data to be fetched

        await confirmation("fetcherCalled") { fetcherCalled in
            let remoteInterface = TestableRemoteInterface { acc, _, _ in
                fetcherCalled()
                return (acc.ncKitAccount, newCaps, newData, .success)
            }

            let result = await remoteInterface.currentCapabilities(account: testAccount)

            #expect(result.error == .success)
            #expect(result.capabilities == newCaps, "Should return newly fetched capabilities.")
            #expect(result.data == newData)
        }

        let actorCache = await RetrievedCapabilitiesActor.shared.getCapabilities(for: testAccount.ncKitAccount)
        #expect(actorCache?.capabilities == newCaps)
        #expect((actorCache?.retrievedAt ?? .distantPast) > staleDate)
    }

    @Test func supportsTrashTrue() async {
        // JSON where files.undelete is true (default mockCapabilitiesJSON)
        let (capsWithTrash, dataWithTrash) = capabilitiesFromMockJSON()
        #expect(capsWithTrash.files?.undelete == true)

        let remoteInterface = TestableRemoteInterface { acc, _, _ in
            (acc.ncKitAccount, capsWithTrash, dataWithTrash, .success)
        }

        await RetrievedCapabilitiesActor.shared.setCapabilities(
            forAccount: testAccount.ncKitAccount,
            capabilities: capsWithTrash, // any capability
            retrievedAt: Date(timeIntervalSinceNow: -(CapabilitiesFetchInterval + 100)) // Stale
        )

        let result = await remoteInterface.supportsTrash(account: testAccount)
        #expect(result == true)
    }

    @Test func supportsTrashFalse() async {
        let jsonNoUndelete = """
        {
            "ocs": {
                "meta": {
                    "status": "ok",
                    "statuscode": 100,
                    "message": "OK"
                },
                "data": {
                    "version": {
                        "major": 31,
                        "minor": 0,
                        "micro": 9,
                        "string": "31.0.9",
                        "edition": "",
                        "extendedSupport": false
                    },
                    "capabilities": {
                        "files": {
                            "undelete": false
                        }
                    }
                }
            }
        }
        """
        let (capsNoTrash, dataNoTrash) = capabilitiesFromMockJSON(jsonString: jsonNoUndelete)
        #expect(capsNoTrash.files?.undelete == false)

        let remoteInterface = TestableRemoteInterface { acc, _, _ in
            await RetrievedCapabilitiesActor.shared.setCapabilities(
                forAccount: acc.ncKitAccount, capabilities: capsNoTrash, retrievedAt: Date()
            )
            return (acc.ncKitAccount, capsNoTrash, dataNoTrash, .success)
        }
        await RetrievedCapabilitiesActor.shared.setCapabilities( // Stale entry
            forAccount: testAccount.ncKitAccount,
            capabilities: capsNoTrash,
            retrievedAt: Date(timeIntervalSinceNow: -(CapabilitiesFetchInterval + 100))
        )

        let result = await remoteInterface.supportsTrash(account: testAccount)
        #expect(result == false)
    }

    @Test func supportsTrashNilCapabilities() async {
        let remoteInterface = TestableRemoteInterface { acc, _, _ in
            (acc.ncKitAccount, nil, nil, .invalidResponseError)
        }

        await RetrievedCapabilitiesActor.shared.setCapabilities(
            forAccount: testAccount.ncKitAccount,
            capabilities: capabilitiesFromMockJSON().0,
            retrievedAt: Date(timeIntervalSinceNow: -(CapabilitiesFetchInterval + 100))
        )

        let result = await remoteInterface.supportsTrash(account: testAccount)
        #expect(!result)
    }

    @Test func supportsTrashNilFilesSection() async {
        let jsonNoFilesSection = """
        {
            "ocs": {
                "meta": {
                    "status": "ok",
                    "statuscode": 100,
                    "message": "OK"
                },
                "data": {
                    "version": {
                        "major": 31,
                        "minor": 0,
                        "micro": 9,
                        "string": "31.0.9",
                        "edition": "",
                        "extendedSupport": false
                    },
                    "capabilities": {
                        "core": {
                            "pollinterval": 60
                        }
                    }
                }
            }
        }
        """
        // This JSON will result in `Capabilities.files` being nil
        let (capsNoFiles, dataNoFiles) = capabilitiesFromMockJSON(jsonString: jsonNoFilesSection)
        #expect(capsNoFiles.files?.undelete != true) // Check our parsing logic

        let remoteInterface = TestableRemoteInterface { acc, _, _ in
            (acc.ncKitAccount, capsNoFiles, dataNoFiles, .success)
        }

        await RetrievedCapabilitiesActor.shared.setCapabilities( // Stale entry
            forAccount: testAccount.ncKitAccount,
            capabilities: capsNoFiles,
            retrievedAt: Date(timeIntervalSinceNow: -(CapabilitiesFetchInterval + 100))
        )

        let result = await remoteInterface.supportsTrash(account: testAccount)
        #expect(!result)
    }

    @Test func supportsTrashHandlesErrorFromCurrentCapabilities() async {
        let remoteInterface = TestableRemoteInterface { acc, _, _ in
            (acc.ncKitAccount, nil, nil, .invalidResponseError)
        }
        // Ensure fetch is triggered
        // (e.g., actor has no data or stale data for testAccount.ncKitAccount)

        let result = await remoteInterface.supportsTrash(account: testAccount)
        #expect(!result, "supportsTrash should return false if currentCapabilities errors.")
    }
}
