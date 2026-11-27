//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

extension Enumerator {
    static func handlePagedReadResults(
        files: [NKFile],
        pageIndex: Int,
        dbManager: FilesDatabaseManager,
        isCancelled: @Sendable () -> Bool = { Task.isCancelled },
        log: any FileProviderLogging
    ) -> (metadatas: [SendableItemMetadata]?, error: NKError?) {
        guard !isCancelled() else { return (nil, .cancelled) }
        let logger = FileProviderLogger(category: "Enumerator", log: log)
        let isFirstPage = pageIndex == 0
        guard !isFirstPage || !files.isEmpty else {
            return (nil, .invalidResponseError)
        }

        let signposter = EnumerationSignposter.signposter
        let convAndPersistState = signposter.beginInterval(
            "ConvertAndPersistPage",
            id: signposter.makeSignpostID(),
            "pageIndex=\(pageIndex) files=\(files.count)"
        )
        var itemCount = 0
        defer { signposter.endInterval("ConvertAndPersistPage", convAndPersistState, "items=\(itemCount)") }

        let clock = ContinuousClock()
        let conversionStart = clock.now
        var target: SendableItemMetadata?
        if isFirstPage {
            // The initial response includes the target. Persist its visit with the page and report only children.
            var metadata = files[0].toItemMetadata()
            if metadata.directory {
                metadata.visitedDirectory = true
            }
            target = metadata
        }

        let children = files.dropFirst(isFirstPage ? 1 : 0).map { $0.toItemMetadata() }
        let dbStart = clock.now
        guard let metadatas = dbManager.persistPaginatedMetadata(
            target: target,
            children: children,
            isCancelled: isCancelled
        ) else {
            return (nil, isCancelled() ? .cancelled : .metadataDatabaseWriteFailed)
        }
        guard !isCancelled() else { return (nil, .cancelled) }

        itemCount = metadatas.count
        let convAccum = dbStart - conversionStart
        let dbSeconds = (clock.now - dbStart).fpSeconds
        let dbRate = dbSeconds > 0 ? Double(itemCount) / dbSeconds : 0
        logger.performance(
            "PERF ConvertAndPersistPage pageIndex=\(pageIndex) items=\(itemCount) conv_s=\(convAccum.fpSeconds) db_s=\(dbSeconds) db_items_per_s=\(dbRate)"
        )

        return (metadatas, nil)
    }

    /// With paginated requests, you do not have a way to know what has changed remotely when
    /// handling the result of an individual PROPFIND request. When handling a paginated read this
    /// therefore only returns the acquired metadatas.
    static func handleDepth1ReadFileOrFolder(
        serverUrl: String,
        dbManager: FilesDatabaseManager,
        files: [NKFile],
        pageIndex: Int?,
        preserveDeletedTarget: Bool,
        requiresCompleteListing: Bool = false,
        isCancelled: @Sendable @escaping () -> Bool = { Task.isCancelled },
        log: any FileProviderLogging
    ) async -> (
        metadatas: [SendableItemMetadata]?,
        changes: ChangeSet?,
        readError: NKError?
    ) {
        let logger = FileProviderLogger(category: "Enumerator", log: log)
        let accountKey = dbManager.account.ncKitAccount

        logger.debug("Starting async conversion of NKFiles for serverUrl: \(serverUrl) for user: \(accountKey)")

        if let pageIndex {
            // Persist off the cooperative pool: see `FilesDatabaseManager.perform(_:)`.
            let (metadatas, error) = await dbManager.perform { dbManager in
                handlePagedReadResults(
                    files: files,
                    pageIndex: pageIndex,
                    dbManager: dbManager,
                    isCancelled: isCancelled,
                    log: log
                )
            }
            return (metadatas, nil, error)
        }

        // Non-paginated path (older servers / change enumeration): conversion is parallelized and the
        // persist is a single batched transaction. Signpost each so its cost is comparable, in a trace,
        // against the paginated path.
        let signposter = EnumerationSignposter.signposter
        let convDirState = signposter.beginInterval(
            "ConvertDirectoryMetadata",
            id: signposter.makeSignpostID(),
            "serverUrl=\(serverUrl, privacy: .public) files=\(files.count)"
        )
        let convertedDirectory = await files.toSendableDirectoryMetadata(requestedDirectoryURL: serverUrl)
        signposter.endInterval("ConvertDirectoryMetadata", convDirState)

        guard let (directory, _, children) = convertedDirectory else {
            logger.error("Failed to convert array of NKFile to directory and files metadata objects!")
            return (nil, nil, .invalidData)
        }

        // STORE DATA FOR CURRENTLY SCANNED DIRECTORY
        guard directory.directory else {
            logger.error("Expected directory metadata but received file metadata!", [.url: serverUrl])
            return (nil, nil, .invalidData)
        }

        let listingMetadata = [directory] + children

        guard !isCancelled() else { return (nil, nil, .cancelled) }
        // Persist off the cooperative pool: see `FilesDatabaseManager.perform(_:)`.
        let (listingWriteResult, batchElapsed) = await dbManager.perform { dbManager in
            let batchClock = ContinuousClock()
            let batchStart = batchClock.now
            let batchWriteState = signposter.beginInterval(
                "Depth1BatchWrite",
                id: signposter.makeSignpostID(),
                "serverUrl=\(serverUrl, privacy: .public) items=\(listingMetadata.count)"
            )
            let listingWriteResult = dbManager.saveDepth1ListingMetadata(
                serverUrl: serverUrl,
                updatedMetadatas: listingMetadata,
                keepExistingDownloadState: true,
                preserveDeletedTarget: preserveDeletedTarget,
                requiresCompleteListing: requiresCompleteListing
            )
            signposter.endInterval("Depth1BatchWrite", batchWriteState)

            return (listingWriteResult, batchClock.now - batchStart)
        }

        // The non-paginated depth-1 write (change / working-set full-folder re-read) is the measured
        // enumeration bottleneck: its per-item logical-dedup scans are O(N²) over a flat folder. Log its
        // wall-clock + items/sec so the effect of the normalizedFileName index is visible in the JSONL
        // (the `Depth1BatchWrite` signpost shows the same in Instruments).
        let batchRate = batchElapsed.fpSeconds > 0 ? Double(listingMetadata.count) / batchElapsed.fpSeconds : 0
        logger.performance(
            "PERF Depth1BatchWrite items=\(listingMetadata.count) write_s=\(batchElapsed.fpSeconds) items_per_s=\(batchRate)",
            [.url: serverUrl]
        )

        guard let listingWriteResult else {
            logger.error("Could not persist directory metadata.", [.url: serverUrl])
            return (nil, nil, .metadataDatabaseWriteFailed)
        }

        let listedItems = listingMetadata.map { listingWriteResult.itemMetadataToReportByID[$0.ocId] ?? $0 }
        return (listedItems, listingWriteResult.changes, nil)
    }

    /// READ THIS CAREFULLY.
    ///
    /// This method supports paginated and non-paginated reads. Handled by the pageSettings argument.
    /// Paginated reads is used by enumerateItems, non-paginated reads is used by enumerateChanges.
    /// Pass the source row for stored URLs so a parser-normalized base can be checked against the row's account.
    ///
    /// Paginated reads WILL NOT HANDLE REMOVAL OF REMOTELY DELETED ITEMS FROM THE LOCAL DATABASE.
    /// Paginated reads WILL ONLY REPORT THE FILES DISCOVERED REMOTELY.
    /// This means that if you decide to use this method to implement change enumeration, you will
    /// have to collect the full results of all the pages before proceeding with discovering what
    /// has changed relative to the state of the local database -- manually!
    ///
    /// Non-paginated reads will update the database with all of the discovered files and folders
    /// that have been found to be created, updated, and deleted. No extra work required.
    static func readServerUrl(
        _ serverUrl: String,
        pageSettings: (page: NSFileProviderPage?, index: Int, size: Int)? = nil,
        sourceMetadata: SendableItemMetadata? = nil,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        domain: NSFileProviderDomain? = nil,
        enumeratedItemIdentifier: NSFileProviderItemIdentifier? = nil,
        expectedDirectoryOcId: String? = nil,
        depth: EnumerateDepth = .targetAndDirectChildren,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void = { _ in },
        isCancelled: @Sendable @escaping () -> Bool = { Task.isCancelled },
        log: any FileProviderLogging
    ) async -> RemoteReadResult {
        let operation = SyncOperations.Operation.current
        let readIsCancelled: @Sendable () -> Bool = { isCancelled() || operation?.isStopped == true }
        guard !readIsCancelled() else { return RemoteReadResult(error: .cancelled) }
        let logger = FileProviderLogger(category: "Enumerator", log: log)

        guard !serverUrl.contains(FilesDatabaseManager.holdingNameSeparator) else {
            logger.info("Not reading a folder that waits to be placed again by a listing.", [.url: serverUrl])
            return RemoteReadResult(error: .parkedFolderPath)
        }

        let account = dbManager.account
        guard account.ownsDAVReadURL(serverUrl, sourceMetadata: sourceMetadata) else {
            logger.error("Skipping DAV read outside the current account roots.", [.url: serverUrl])
            return RemoteReadResult(error: .invalidData)
        }

        let isCurrent = await dbManager.perform { $0.isForCurrentDAVAccount() }
        guard isCurrent else { return RemoteReadResult(error: .davAccountChanged) }
        guard !readIsCancelled() else { return RemoteReadResult(error: .cancelled) }

        // The database can switch accounts while the server request or its write is in flight.
        func resultForCurrentDAVAccount(_ result: RemoteReadResult) async -> RemoteReadResult {
            let isCurrent = await dbManager.perform { $0.isForCurrentDAVAccount() }
            guard !readIsCancelled() else { return RemoteReadResult(error: .cancelled) }
            return isCurrent ? result : RemoteReadResult(error: .davAccountChanged)
        }

        logger.debug("Starting to read server URL.", [.url: serverUrl])

        let options: NKRequestOptions = if let pageSettings {
            .init(
                page: pageSettings.page,
                offset: pageSettings.index * pageSettings.size,
                count: pageSettings.size
            )
        } else {
            .init()
        }

        // Signpost + wall-clock the network read in isolation so a trace (or the JSONL PERF line) can
        // attribute enumeration latency to the paginated PROPFIND (server-bound) versus the local
        // conversion + database persistence (CPU-bound). begin/end stay in this one function scope so the
        // non-Sendable interval state never crosses the `await`'s potential thread hop.
        let pageIndexForLog = pageSettings?.index ?? 0
        let signposter = EnumerationSignposter.signposter
        let propfindSignpostID = signposter.makeSignpostID()
        let propfindState = signposter.beginInterval(
            "PROPFIND",
            id: propfindSignpostID,
            "serverUrl=\(serverUrl, privacy: .public) index=\(pageIndexForLog) depth=\(depth.rawValue, privacy: .public)"
        )
        let networkClock = ContinuousClock()
        let networkStart = networkClock.now

        let registeringTaskHandler: @Sendable (URLSessionTask) -> Void = { task in
            operation?.registerReadTask(task)
            taskHandler(task)
            if let domain, let enumeratedItemIdentifier {
                NSFileProviderManager(for: domain)?.register(
                    task,
                    forItemWithIdentifier: enumeratedItemIdentifier,
                    completionHandler: { _ in }
                )
            }
        }

        let (_, enumeratedFiles, data, error) = await remoteInterface.enumerate(
            remotePath: serverUrl,
            depth: depth,
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            account: account,
            options: options,
            taskHandler: registeringTaskHandler
        )

        var files = enumeratedFiles
        let networkElapsed = networkClock.now - networkStart
        signposter.endInterval("PROPFIND", propfindState, "files=\(files.count)")
        logger.performance(
            "PERF PROPFIND index=\(pageIndexForLog) net_s=\(networkElapsed.fpSeconds) files=\(files.count) depth=\(depth.rawValue)",
            [.url: serverUrl]
        )

        guard !readIsCancelled() else { return RemoteReadResult(error: .cancelled) }
        guard error == .success else {
            logger.error("Read of URL did fail.", [.error: error, .url: serverUrl])
            // A 404 at the account's Files root means the domain could not be read. Let Finder retry.
            if error.isNotFoundError,
               account.davPath(forURL: serverUrl)?.isFilesRoot == true
            {
                return RemoteReadResult(error: .invalidData)
            }
            return await resultForCurrentDAVAccount(RemoteReadResult(error: error))
        }

        guard let data else {
            logger.error("\(depth.rawValue) depth read of url \(serverUrl) did not return data.")
            return await resultForCurrentDAVAccount(RemoteReadResult(error: error))
        }

        if let expectedDirectoryOcId {
            // A path may now refer to a different item from the one this download owns.
            let target = files.first { $0.matchesRequestedDirectoryURL(serverUrl) }?.toItemMetadata()
            guard let target, target.directory, target.ocId == expectedDirectoryOcId, target.account == account.ncKitAccount else {
                logger.error("Read directory no longer matches the download's item.", [.item: expectedDirectoryOcId, .url: serverUrl])
                return RemoteReadResult(error: .invalidData)
            }
        }

        // This will be nil if the page settings were also nil, as the server will not give us the
        // pagination-related headers.
        let nextPage = EnumeratorPageResponse(nkResponseData: data, index: (pageSettings?.index ?? 0) + 1, log: log)

        guard let receivedFile = files.first else {
            logger.error("Received no items.", [.url: serverUrl])
            // This is technically possible when doing a paginated request with the index too high.
            // It's technically not an error reply.
            return await resultForCurrentDAVAccount(RemoteReadResult(metadatas: [], nextPage: nextPage))
        }

        func finishTargetRead(_ metadata: SendableItemMetadata) async -> RemoteReadResult {
            let resolved: (metadata: SendableItemMetadata, isNew: Bool)
            do {
                resolved = try await dbManager.addItemMetadataFromRemoteRead(
                    metadata,
                    requestedURL: serverUrl,
                    isCancelled: readIsCancelled
                )
            } catch is CancellationError {
                return RemoteReadResult(error: .cancelled)
            } catch let error as NSFileProviderError where error.code == .cannotSynchronize {
                return RemoteReadResult(error: .davAccountChanged)
            } catch let error as NKError {
                return RemoteReadResult(error: error)
            } catch {
                // A single-item lookup can still return the server result if caching fails.
                logger.error("Could not persist remote item metadata.", [.item: metadata.ocId, .error: error])
                resolved = await dbManager.perform { $0.itemMetadataFromRemoteReadWithoutSaving(metadata) }
                guard !readIsCancelled() else { return RemoteReadResult(error: .cancelled) }
            }
            let changes = resolved.isNew
                ? ChangeSet(created: [resolved.metadata])
                : ChangeSet(updated: [resolved.metadata])
            let result = RemoteReadResult(metadatas: [resolved.metadata], changes: changes, nextPage: nextPage)
            return await resultForCurrentDAVAccount(result)
        }

        // Follow-up pages begin with a child instead of the target.
        let isFollowUpPaginatedRequest = (pageSettings?.page != nil && pageSettings?.index ?? 0 > 0)
        var responseIsDirectory: Bool {
            receivedFile.directory
                || serverUrl == dbManager.account.davFilesUrl
                || receivedFile.matchesResponseItemURL(dbManager.account.davFilesUrl + "/.")
                || (receivedFile.fileName == NextcloudKit.shared.nkCommonInstance.rootFileName && receivedFile.serverUrl == dbManager.account.davFilesUrl)
        }

        if !isFollowUpPaginatedRequest, !responseIsDirectory {
            return await finishTargetRead(receivedFile.toItemMetadata())
        }

        // A folder stored elsewhere can be a second mount with the same identifier (#10106).
        let listsTarget = depth != .target && !isFollowUpPaginatedRequest
        guard let secondMounts = await secondMountedDirectories(
            among: listsTarget ? Array(files.dropFirst()) : files,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            taskHandler: registeringTaskHandler,
            isCancelled: readIsCancelled,
            logger: logger
        ) else {
            return RemoteReadResult(error: .cancelled)
        }
        files.removeAll { secondMounts[$0.ocId] != nil }

        if depth == .target {
            if let storedMetadata = secondMounts[receivedFile.ocId] {
                let result = RemoteReadResult(metadatas: [storedMetadata], changes: ChangeSet(), nextPage: nextPage)
                return await resultForCurrentDAVAccount(result)
            }

            return await finishTargetRead(receivedFile.toItemMetadata())
        } else if depth == .targetAndDirectChildren {
            let depth1Read = await handleDepth1ReadFileOrFolder(
                serverUrl: serverUrl,
                dbManager: dbManager,
                files: files,
                pageIndex: pageSettings?.index,
                preserveDeletedTarget: expectedDirectoryOcId != nil,
                requiresCompleteListing: expectedDirectoryOcId != nil,
                isCancelled: readIsCancelled,
                log: logger.log
            )

            let result = RemoteReadResult(
                metadatas: depth1Read.metadatas,
                changes: depth1Read.changes,
                nextPage: nextPage,
                error: depth1Read.readError
            )
            return await resultForCurrentDAVAccount(result)
        } else if let pageIndex = pageSettings?.index {
            let (metadatas, error) = handlePagedReadResults(
                files: files,
                pageIndex: pageIndex,
                dbManager: dbManager,
                isCancelled: readIsCancelled,
                log: log
            )
            let result = RemoteReadResult(metadatas: metadatas, nextPage: nextPage, error: error)
            return await resultForCurrentDAVAccount(result)
        } else {
            // Infinite depth unpaged reads are a bad idea
            logger.error("Infinite depth read requested.")
            return RemoteReadResult(error: .cancelled)
        }
    }

    ///
    /// The listed folders to keep at their stored path, keyed by identifier, because the server still lists them there.
    ///
    /// Only a folder stored at the root or below another live parent can be a second mount. A single such folder below a parent
    /// is read at its stored path, several are checked with one listing of their stored parent, and a folder the server no
    /// longer has there, or whose check fails, was moved. Up to ``workingSetScanConcurrency`` stored parents are checked at once.
    ///
    /// - Returns: `nil` when the read was cancelled.
    ///
    private static func secondMountedDirectories(
        among files: [NKFile],
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void,
        isCancelled: @Sendable () -> Bool,
        logger: FileProviderLogger
    ) async -> [String: SendableItemMetadata]? {
        let listedDirectories = files.filter(\.directory)

        guard !listedDirectories.isEmpty else {
            return [:]
        }

        let candidates = await dbManager.perform { manager in
            let storedByOcId = manager.itemMetadatas(ocIds: listedDirectories.map(\.ocId)) ?? [:]

            return listedDirectories.compactMap { listed -> SendableItemMetadata? in
                guard let stored = storedByOcId[listed.ocId],
                      stored.directory,
                      !stored.deleted,
                      !stored.isFilesRoot,
                      !stored.isTrashed,
                      !stored.isAtHoldingPath,
                      !account.hasSameDAVPath(stored.serverUrl, listed.serverUrl),
                      account.davPath(forURL: stored.serverUrl)?.isFilesRoot == true
                      || manager.parentDirectoryMetadataForItem(stored).map({ !$0.deleted }) == true
                else {
                    return nil
                }

                return stored
            }
        }

        var secondMounts: [String: SendableItemMetadata] = [:]

        await withTaskGroup(of: [SendableItemMetadata].self) { group in
            for (index, (storedParentPath, storedDirectories)) in Dictionary(grouping: candidates, by: \.serverUrl).enumerated() {
                if index >= workingSetScanConcurrency, let kept = await group.next() {
                    kept.forEach { secondMounts[$0.ocId] = $0 }
                }

                guard !isCancelled() else {
                    group.cancelAll()
                    break
                }

                group.addTask {
                    await foldersStillListed(among: storedDirectories, storedAt: storedParentPath, account: account, remoteInterface: remoteInterface, taskHandler: taskHandler, logger: logger)
                }
            }

            for await kept in group {
                kept.forEach { secondMounts[$0.ocId] = $0 }
            }
        }

        return isCancelled() ? nil : secondMounts
    }

    /// The folders among `storedDirectories`, all stored below `storedParentPath`, that the server still lists there.
    private static func foldersStillListed(
        among storedDirectories: [SendableItemMetadata],
        storedAt storedParentPath: String,
        account: Account,
        remoteInterface: RemoteInterface,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void,
        logger: FileProviderLogger
    ) async -> [SendableItemMetadata] {
        // A single folder is read on its own, so a large stored parent is not downloaded on every listing.
        let checksSingleFolder = storedDirectories.count == 1
        let checkedPath = checksSingleFolder ? storedDirectories[0].remotePath() : storedParentPath
        let (listedAtStoredLocation, error) = await listedIdentifiers(
            at: checkedPath,
            depth: checksSingleFolder ? .target : .targetAndDirectChildren,
            account: account,
            sourceMetadata: storedDirectories.first,
            remoteInterface: remoteInterface,
            taskHandler: taskHandler
        )

        guard error == .success else {
            logger.info("Treated folders as moved because their stored location could not be read.", [.url: checkedPath, .error: error])
            return []
        }

        let kept = storedDirectories.filter { listedAtStoredLocation.contains($0.ocId) }

        for stored in kept {
            logger.info("Kept a folder at its stored path because the server still lists it there.", [.item: stored.ocId, .url: storedParentPath])
        }

        return kept
    }

    /// The identifiers the server lists at `remotePath` to the given depth, or the error of the read.
    static func listedIdentifiers(
        at remotePath: String,
        depth: EnumerateDepth,
        account: Account,
        sourceMetadata: SendableItemMetadata? = nil,
        remoteInterface: RemoteInterface,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (ocIds: Set<String>, error: NKError) {
        guard account.ownsDAVReadURL(remotePath, sourceMetadata: sourceMetadata) else {
            return ([], .invalidData)
        }

        let (_, files, _, error) = await remoteInterface.enumerate(
            remotePath: remotePath,
            depth: depth,
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            account: account,
            options: .init(),
            taskHandler: taskHandler
        )

        return (Set(files.map(\.ocId)), error)
    }
}
