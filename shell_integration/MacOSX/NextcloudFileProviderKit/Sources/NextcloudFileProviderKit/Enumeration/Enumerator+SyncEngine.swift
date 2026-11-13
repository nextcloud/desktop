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
        // First PROPFIND contains the target item, but we do not want to report this in the
        // retrieved metadatas (the enumeration observers don't expect you to enumerate the
        // target item, hence why we always strip the target item out)
        let startIndex = pageIndex > 0 ? 0 : 1
        if pageIndex == 0 {
            guard let firstFile = files.first else { return (nil, .invalidResponseError) }
            var metadata = firstFile.toItemMetadata()

            if metadata.directory {
                metadata.visitedDirectory = true
            }
            do {
                _ = try dbManager.persistItemMetadataFromPagedListing(metadata, preserveVisitedDirectory: !metadata.directory)
            } catch {
                logger.error("Could not persist the listed directory.", [.item: metadata.ocId, .error: error])
                return (nil, .invalidData)
            }
        }

        // Persist children — including those on follow-up pages — while
        // carrying over any local-only state previously set on existing rows
        // (e.g. `keepDownloaded` from a recursive "Always keep downloaded"
        // enable). A plain `addItemMetadata` would clobber those flags via
        // a full-row upsert, which is the root cause of #9923.
        //
        // Use the merged metadata returned by the helper for both the DB
        // write and the value reported back to the framework. Returning the
        // pre-merge fresh-from-server metadata would tell the framework
        // `keepDownloaded == false` for items that are pinned in the
        // database, leaving the OS view (`isKeepDownloaded`, `contentPolicy`)
        // out of sync with the local truth.
        //
        // Conversion and persistence are timed separately (convAccum / dbAccum) so the JSONL PERF
        // line splits CPU spent building metadata from CPU spent in the database. Today each item opens its
        // own write transaction inside `persistItemMetadataFromPagedListing`; `db_items_per_s` is the
        // throughput number to watch, and the enclosing `ConvertAndPersistPage` signpost bounds the
        // whole page for Instruments. (Phase 2 batches these into one transaction per page.)
        let signposter = EnumerationSignposter.signposter
        let convAndPersistState = signposter.beginInterval(
            "ConvertAndPersistPage",
            id: signposter.makeSignpostID(),
            "pageIndex=\(pageIndex) files=\(files.count)"
        )
        let clock = ContinuousClock()
        var convAccum: Duration = .zero
        var dbAccum: Duration = .zero
        var metadatas: [SendableItemMetadata] = []
        metadatas.reserveCapacity(max(0, files.count - startIndex))
        defer { signposter.endInterval("ConvertAndPersistPage", convAndPersistState, "items=\(metadatas.count)") }

        for file in files[startIndex...] {
            guard !isCancelled() else { return (nil, .cancelled) }
            let convStart = clock.now
            let itemMetadata = file.toItemMetadata()
            convAccum += clock.now - convStart

            let dbStart = clock.now
            do {
                if let savedMetadata = try dbManager.persistItemMetadataFromPagedListing(
                    itemMetadata,
                    isChildInDirectoryListing: true
                ) {
                    metadatas.append(savedMetadata)
                }
            } catch {
                logger.error("Could not persist the listed item.", [.item: itemMetadata.ocId, .error: error])
                return (nil, .invalidData)
            }
            dbAccum += clock.now - dbStart
        }

        let itemCount = metadatas.count
        let dbSeconds = dbAccum.fpSeconds
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
        readError: NKError?,
        omittedPendingUploadIDs: Set<String>
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
            return (metadatas, nil, error, [])
        }

        // Non-paginated path (older servers / change enumeration): conversion is parallelized and the
        // persist is a single batched transaction. Signpost each so its cost is comparable, in a trace,
        // against the paginated path's per-item behavior.
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
            return (nil, nil, .invalidData, [])
        }

        // STORE DATA FOR CURRENTLY SCANNED DIRECTORY
        guard directory.directory else {
            logger.error("Expected directory metadata but received file metadata!", [.url: serverUrl])
            return (nil, nil, .invalidData, [])
        }

        let listingMetadata = [directory] + children

        guard !isCancelled() else { return (nil, nil, .cancelled, []) }
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
            return (nil, nil, .metadataDatabaseWriteFailed, [])
        }

        let listedItems = listingMetadata.compactMap { metadata -> SendableItemMetadata? in
            guard !listingWriteResult.itemIDsToOmitFromListing.contains(metadata.ocId) else { return nil }
            return listingWriteResult.itemMetadataToReportByID[metadata.ocId] ?? metadata
        }
        return (listedItems, listingWriteResult.changes, nil, listingWriteResult.itemIDsToOmitFromListing)
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

        let (_, files, data, error) = await remoteInterface.enumerate(
            remotePath: serverUrl,
            depth: depth,
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            account: account,
            options: options,
            taskHandler: { task in
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
        )

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
               let davPath = account.davRelativePath(forURL: serverUrl),
               Account.isStoredFilesRootLocation(davPath) {
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
        if depth == .target || (!isFollowUpPaginatedRequest && !responseIsDirectory) {
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
                omittedPendingUploadIDs: depth1Read.omittedPendingUploadIDs,
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
}
