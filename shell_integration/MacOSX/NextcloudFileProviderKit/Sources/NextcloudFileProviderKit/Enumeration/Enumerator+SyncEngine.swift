//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

extension Enumerator {
    static func handlePagedReadResults(
        files: [NKFile], pageIndex: Int, dbManager: FilesDatabaseManager, log: any FileProviderLogging
    ) -> (metadatas: [SendableItemMetadata]?, error: NKError?) {
        // First PROPFIND contains the target item, but we do not want to report this in the
        // retrieved metadatas (the enumeration observers don't expect you to enumerate the
        // target item, hence why we always strip the target item out)
        let startIndex = pageIndex > 0 ? 0 : 1
        if pageIndex == 0 {
            guard let firstFile = files.first else { return (nil, .invalidResponseError) }
            var metadata = firstFile.toItemMetadata()

            if metadata.directory {
                // Mark the directory as visited in this request, then persist
                // while preserving the rest of the local-only state. Skip
                // preservation of `visitedDirectory` so the just-set `true`
                // is not overwritten by the existing row's value.
                metadata.visitedDirectory = true
                dbManager.addItemMetadataPreservingLocalState(metadata, preserveVisitedDirectory: false)
            } else {
                dbManager.addItemMetadataPreservingLocalState(metadata)
            }
        }

        // Persist children — including those on follow-up pages — while
        // carrying over any local-only state previously set on existing rows
        // (e.g. `keepDownloaded` from a recursive "Always keep downloaded"
        // enable). A plain `addItemMetadata` would clobber those flags via
        // Realm's `update: .all`, which is the root cause of #9923.
        //
        // Use the merged metadata returned by the helper for both the DB
        // write and the value reported back to the framework. Returning the
        // pre-merge fresh-from-server metadata would tell the framework
        // `keepDownloaded == false` for items that are pinned in the
        // database, leaving the OS view (`isKeepDownloaded`, `contentPolicy`)
        // out of sync with the local truth.
        //
        // Conversion and persistence are timed separately (convAccum / dbAccum) so the JSONL PERF
        // line splits CPU spent building metadata from CPU spent in Realm. Today each item opens its
        // own write transaction inside `addItemMetadataPreservingLocalState`; `db_items_per_s` is the
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

        for file in files[startIndex...] {
            let convStart = clock.now
            let itemMetadata = file.toItemMetadata()
            convAccum += clock.now - convStart

            let dbStart = clock.now
            metadatas.append(dbManager.addItemMetadataPreservingLocalState(itemMetadata))
            dbAccum += clock.now - dbStart
        }

        signposter.endInterval("ConvertAndPersistPage", convAndPersistState, "items=\(metadatas.count)")

        let itemCount = metadatas.count
        let dbSeconds = dbAccum.fpSeconds
        let dbRate = dbSeconds > 0 ? Double(itemCount) / dbSeconds : 0
        FileProviderLogger(category: "Enumerator", log: log).performance(
            "PERF ConvertAndPersistPage pageIndex=\(pageIndex) items=\(itemCount) conv_s=\(convAccum.fpSeconds) db_s=\(dbSeconds) db_items_per_s=\(dbRate)"
        )

        return (metadatas, nil)
    }

    /// With paginated requests, you do not have a way to know what has changed remotely when
    /// handling the result of an individual PROPFIND request. When handling a paginated read this
    /// therefore only returns the acquired metadatas.
    static func handleDepth1ReadFileOrFolder(
        serverUrl: String,
        account: Account,
        dbManager: FilesDatabaseManager,
        files: [NKFile],
        pageIndex: Int?,
        log: any FileProviderLogging
    ) async -> (
        metadatas: [SendableItemMetadata]?,
        changes: ChangeSet?,
        readError: NKError?
    ) {
        let logger = FileProviderLogger(category: "Enumerator", log: log)

        logger.debug("Starting async conversion of NKFiles for serverUrl: \(serverUrl) for user: \(account.ncKitAccount)")

        if let pageIndex {
            let (metadatas, error) =
                handlePagedReadResults(files: files, pageIndex: pageIndex, dbManager: dbManager, log: log)
            return (metadatas, nil, error)
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
        let convertedDirectory = await files.toSendableDirectoryMetadata(account: account, directoryToRead: serverUrl)
        signposter.endInterval("ConvertDirectoryMetadata", convDirState)

        guard var (directory, _, files) = convertedDirectory else {
            logger.error("Failed to convert array of NKFile to directory and files metadata objects!")
            return (nil, nil, .invalidData)
        }

        // STORE DATA FOR CURRENTLY SCANNED DIRECTORY
        guard directory.directory else {
            logger.error("Expected directory metadata but received file metadata!", [.url: serverUrl])
            return (nil, nil, .invalidData)
        }

        if let existingMetadata = dbManager.itemMetadata(ocId: directory.ocId) {
            directory.downloaded = existingMetadata.downloaded
            directory.keepDownloaded = existingMetadata.keepDownloaded
        }

        directory.visitedDirectory = true

        files.insert(directory, at: 0)

        let batchClock = ContinuousClock()
        let batchStart = batchClock.now
        let batchWriteState = signposter.beginInterval(
            "Depth1BatchWrite",
            id: signposter.makeSignpostID(),
            "serverUrl=\(serverUrl, privacy: .public) items=\(files.count)"
        )
        let changes = dbManager.depth1ReadUpdateItemMetadatas(
            account: account.ncKitAccount,
            serverUrl: serverUrl,
            updatedMetadatas: files,
            keepExistingDownloadState: true
        )
        signposter.endInterval("Depth1BatchWrite", batchWriteState)

        // The non-paginated depth-1 write (change / working-set full-folder re-read) is the measured
        // enumeration bottleneck: its per-item logical-dedup scans are O(N²) over a flat folder. Log its
        // wall-clock + items/sec so the effect of the normalizedFileName index is visible in the JSONL
        // (the `Depth1BatchWrite` signpost shows the same in Instruments).
        let batchElapsed = batchClock.now - batchStart
        let batchRate = batchElapsed.fpSeconds > 0 ? Double(files.count) / batchElapsed.fpSeconds : 0
        logger.performance(
            "PERF Depth1BatchWrite items=\(files.count) write_s=\(batchElapsed.fpSeconds) items_per_s=\(batchRate)",
            [.url: serverUrl]
        )

        return (files, changes, nil)
    }

    /// READ THIS CAREFULLY.
    ///
    /// This method supports paginated and non-paginated reads. Handled by the pageSettings argument.
    /// Paginated reads is used by enumerateItems, non-paginated reads is used by enumerateChanges.
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
        account: Account,
        remoteInterface: RemoteInterface,
        dbManager: FilesDatabaseManager,
        domain: NSFileProviderDomain? = nil,
        enumeratedItemIdentifier: NSFileProviderItemIdentifier? = nil,
        depth: EnumerateDepth = .targetAndDirectChildren,
        log: any FileProviderLogging
    ) async -> RemoteReadResult {
        let logger = FileProviderLogger(category: "Enumerator", log: log)

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
        // conversion + Realm persistence (CPU-bound). begin/end stay in this one function scope so the
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

        guard error == .success else {
            logger.error("Read of URL did fail.", [.error: error, .url: serverUrl])
            return RemoteReadResult(error: error)
        }

        guard let data else {
            logger.error("\(depth.rawValue) depth read of url \(serverUrl) did not return data.")
            return RemoteReadResult(error: error)
        }

        // This will be nil if the page settings were also nil, as the server will not give us the
        // pagination-related headers.
        let nextPage = EnumeratorPageResponse(nkResponseData: data, index: (pageSettings?.index ?? 0) + 1, log: log)

        guard let receivedFile = files.first else {
            logger.error("Received no items.", [.url: serverUrl])
            // This is technically possible when doing a paginated request with the index too high.
            // It's technically not an error reply.
            return RemoteReadResult(metadatas: [], nextPage: nextPage)
        }

        // Generally speaking a PROPFIND will provide the target of the PROPFIND as the first result
        // That is NOT the case for paginated results with offsets
        let isFollowUpPaginatedRequest = (pageSettings?.page != nil && pageSettings?.index ?? 0 > 0)
        if !isFollowUpPaginatedRequest {
            guard receivedFile.directory ||
                serverUrl == dbManager.account.davFilesUrl ||
                receivedFile.fullUrlMatches(dbManager.account.davFilesUrl + "/.") ||
                (receivedFile.fileName == NextcloudKit.shared.nkCommonInstance.rootFileName && receivedFile.serverUrl == dbManager.account.davFilesUrl)
            else {
                logger.debug("Read item is a file, converting.", [.url: serverUrl])
                var metadata = receivedFile.toItemMetadata()
                let existing = dbManager.itemMetadata(ocId: metadata.ocId)
                let isNew = existing == nil
                let newItems: [SendableItemMetadata] = isNew ? [metadata] : []
                metadata.lockToken = existing?.lockToken
                let updatedItems: [SendableItemMetadata] = isNew ? [] : [metadata]
                metadata.downloaded = existing?.downloaded == true

                if let existing {
                    metadata.keepDownloaded = existing.keepDownloaded
                } else {
                    // Genuinely new item: inherit the parent's pin so the
                    // overlay and `contentPolicy` match its siblings (#10054).
                    metadata.keepDownloaded = dbManager.inheritedKeepDownloaded(for: metadata)
                }

                dbManager.addItemMetadata(metadata)
                return RemoteReadResult(
                    metadatas: [metadata],
                    changes: ChangeSet(created: newItems, updated: updatedItems),
                    nextPage: nextPage
                )
            }
        }

        if depth == .target {
            var metadata = receivedFile.toItemMetadata()
            let existing = dbManager.itemMetadata(ocId: metadata.ocId)
            let isNew = existing == nil
            let updatedMetadatas = isNew ? [] : [metadata]
            let newMetadatas = isNew ? [metadata] : []

            metadata.lockToken = existing?.lockToken
            metadata.visitedDirectory = existing?.visitedDirectory == true
            metadata.downloaded = existing?.downloaded == true

            if let existing {
                metadata.keepDownloaded = existing.keepDownloaded
            } else {
                // Genuinely new item: inherit the parent's pin so the
                // overlay and `contentPolicy` match its siblings (#10054).
                metadata.keepDownloaded = dbManager.inheritedKeepDownloaded(for: metadata)
            }

            dbManager.addItemMetadata(metadata)

            return RemoteReadResult(
                metadatas: [metadata],
                changes: ChangeSet(created: newMetadatas, updated: updatedMetadatas),
                nextPage: nextPage
            )
        } else if depth == .targetAndDirectChildren {
            let depth1Read = await handleDepth1ReadFileOrFolder(
                serverUrl: serverUrl,
                account: account,
                dbManager: dbManager,
                files: files,
                pageIndex: pageSettings?.index,
                log: logger.log
            )

            return RemoteReadResult(
                metadatas: depth1Read.metadatas,
                changes: depth1Read.changes,
                nextPage: nextPage,
                error: depth1Read.readError
            )
        } else if let pageIndex = pageSettings?.index {
            let (metadatas, error) = handlePagedReadResults(
                files: files, pageIndex: pageIndex, dbManager: dbManager, log: log
            )
            return RemoteReadResult(metadatas: metadatas, nextPage: nextPage, error: error)
        } else {
            // Infinite depth unpaged reads are a bad idea
            logger.error("Infinite depth read requested.")
            return RemoteReadResult(error: .cancelled)
        }
    }
}
