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

    private func makeNetworkTask() throws -> (URLSession, URLSessionTask) {
        let session = URLSession(configuration: .ephemeral)
        let task = try session.dataTask(with: #require(URL(string: "https://example.invalid/capabilities")))
        return (session, task)
    }

    let testAccount = Account(user: "a1", id: "1", serverUrl: "example.com", password: "pass")
    let otherAccount = Account(user: "a2", id: "2", serverUrl: "example.com", password: "word")

    @Test func supportsTrashUsesSharedCapabilitiesRequest() async throws {
        await RetrievedCapabilitiesActor.shared.reset()
        let (session, networkTask) = try makeNetworkTask()
        defer { session.invalidateAndCancel() }
        let (capabilities, data) = capabilitiesFromMockJSON()
        let remote = TestableRemoteInterface { account, _, taskHandler in
            taskHandler(networkTask)
            return (account.ncKitAccount, capabilities, data, .success)
        }
        let supported = await remote.supportsTrash(account: testAccount)
        #expect(supported)
        #expect(networkTask.state == .suspended)
    }

    @Test(arguments: [false, true])
    func cancellingCapabilitiesCallersPreservesRequestUntilLastCaller(cancelRemainingCaller: Bool) async throws {
        await RetrievedCapabilitiesActor.shared.reset()
        let (session, networkTask) = try makeNetworkTask()
        defer { session.invalidateAndCancel() }
        let (capabilities, data) = capabilitiesFromMockJSON()
        let (started, startedContinuation) = AsyncStream<Void>.makeStream()
        let (finish, finishContinuation) = AsyncStream<Void>.makeStream()
        defer {
            startedContinuation.finish()
            finishContinuation.finish()
        }
        try await confirmation("One shared capabilities request", expectedCount: 1) { fetched in
            let remote = TestableRemoteInterface { account, _, taskHandler in
                fetched()
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
            let (waiting, waitingContinuation) = AsyncStream<Void>.makeStream()
            defer { waitingContinuation.finish() }
            let second = Task {
                await RetrievedCapabilitiesActor.shared.currentCapabilities(
                    forAccount: testAccount.ncKitAccount, onWaiting: { waitingContinuation.yield(()) }
                ) { _ in
                    Issue.record("The remaining caller must share the first request")
                    return (testAccount.ncKitAccount, nil, nil, .invalidResponseError)
                }
            }
            defer { second.cancel() }
            try #require(try await nextTestValue(from: waiting) != nil)
            progress.cancel()

            let cancelled = try await testTaskValue(of: first)
            #expect(cancelled.error.errorCode == NSURLErrorCancelled)
            #expect(cancelled.capabilities == nil)
            #expect(networkTask.state == .suspended)
            if cancelRemainingCaller {
                second.cancel()
                #expect(try await testTaskValue(of: second).error == .cancelled)
                #expect(networkTask.state == .canceling || networkTask.state == .completed)
                #expect(await RetrievedCapabilitiesActor.shared.getCapabilities(for: testAccount.ncKitAccount) == nil)
                return
            }
            finishContinuation.yield(())
            let result = try await testTaskValue(of: second)
            #expect(result.error == .success)
            #expect(result.capabilities == capabilities)
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
        await actor.reset()
        let (session, networkTask) = try makeNetworkTask()
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
        #expect(await actor.ongoingFetches.isEmpty)
        if !registerAfterCancellation {
            #expect(networkTask.state == .canceling || networkTask.state == .completed)
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
        #expect(networkTask.state == .canceling || networkTask.state == .completed)
        if finishReplacementFirst {
            #expect(await actor.getCapabilities(for: testAccount.ncKitAccount)?.capabilities == newCapabilities)
        } else {
            #expect(await actor.getCapabilities(for: testAccount.ncKitAccount) == nil)
            #expect(await actor.ongoingFetches.contains(testAccount.ncKitAccount))
            releaseNewContinuation.yield(())
            #expect(try await testTaskValue(of: replacement).capabilities == newCapabilities)
        }
        #expect(await actor.ongoingFetches.isEmpty)
        let cacheOnlyRemote = TestableRemoteInterface { account, _, _ in
            Issue.record("The replacement's successful capabilities must remain cached")
            return (account.ncKitAccount, nil, nil, .invalidResponseError)
        }
        #expect(await cacheOnlyRemote.currentCapabilities(account: testAccount).capabilities == newCapabilities)
    }

    @Test func cancelledCapabilitiesDoNotStartAnotherFetch() async {
        await RetrievedCapabilitiesActor.shared.reset()
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
        await RetrievedCapabilitiesActor.shared.reset()
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
        await RetrievedCapabilitiesActor.shared.reset()

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
        await RetrievedCapabilitiesActor.shared.reset()

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

    @Test func currentCapabilitiesAwaitsAndUsesCache() async {
        await RetrievedCapabilitiesActor.shared.reset()

        let (cachedCaps, cachedData) = capabilitiesFromMockJSON()

        let remoteInterface = TestableRemoteInterface { acc, _, _ in
            Issue.record("fetchCapabilities should NOT be called when cache is fresh after await.")
            return (acc.ncKitAccount, cachedCaps, cachedData, .success)
        }

        // 1. Simulate an external process starting a fetch for testAccount
        await RetrievedCapabilitiesActor.shared.setOngoingFetch(forAccount: testAccount.ncKitAccount, ongoing: true)

        await confirmation("currentCapabilitiesReturned") { currentCapabilitiesReturned in
            let currentCapabilitiesTask = Task { @Sendable in
                // 2. This call to currentCapabilities should await the ongoing fetch.
                let result = await remoteInterface.currentCapabilities(account: testAccount)
                currentCapabilitiesReturned()
                // Assertions on the result will be done after the task.
                #expect(result.capabilities == cachedCaps)
                #expect(result.error == .success)
            }

            // 3. Now, the "external" fetch completes and populates the cache.
            await RetrievedCapabilitiesActor.shared.setCapabilities(
                forAccount: testAccount.ncKitAccount,
                capabilities: cachedCaps,
                retrievedAt: Date() // Fresh date
            )

            await RetrievedCapabilitiesActor.shared.setOngoingFetch(forAccount: testAccount.ncKitAccount, ongoing: false)

            await currentCapabilitiesTask.value
        }
    }

    @Test func supportsTrashTrue() async {
        await RetrievedCapabilitiesActor.shared.reset() // Reset shared actor

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
        await RetrievedCapabilitiesActor.shared.reset()
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
        await RetrievedCapabilitiesActor.shared.reset()
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
        await RetrievedCapabilitiesActor.shared.reset()
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
        await RetrievedCapabilitiesActor.shared.reset()

        let remoteInterface = TestableRemoteInterface { acc, _, _ in
            (acc.ncKitAccount, nil, nil, .invalidResponseError)
        }
        // Ensure fetch is triggered
        // (e.g., actor has no data or stale data for testAccount.ncKitAccount)

        let result = await remoteInterface.supportsTrash(account: testAccount)
        #expect(!result, "supportsTrash should return false if currentCapabilities errors.")
    }
}
