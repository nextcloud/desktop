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
                dbManager.addItemMetadataPreservingLocalState(metadata, preserveVisitedDirectory: false, keepPermissionsListedByParent: true)
            } else {
                dbManager.addItemMetadataPreservingLocalState(metadata)
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
        preserveDeletedTarget: Bool,
        log: any FileProviderLogging
    ) async -> (
        metadatas: [SendableItemMetadata]?,
        changes: ChangeSet?,
        readError: NKError?
    ) {
        let logger = FileProviderLogger(category: "Enumerator", log: log)

        logger.debug("Starting async conversion of NKFiles for serverUrl: \(serverUrl) for user: \(account.ncKitAccount)")

        if let pageIndex {
            // Persist off the cooperative pool: see `FilesDatabaseManager.perform(_:)`.
            let (metadatas, error) = await dbManager.perform { dbManager in
                handlePagedReadResults(files: files, pageIndex: pageIndex, dbManager: dbManager, log: log)
            }
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

        // Both database calls run off the cooperative pool: see `FilesDatabaseManager.perform(_:)`.
        let directoryOcId = directory.ocId
        if let existingMetadata = await dbManager.perform({ $0.itemMetadata(ocId: directoryOcId) }) {
            directory.downloaded = existingMetadata.downloaded
            directory.keepDownloaded = existingMetadata.keepDownloaded
        }

        directory.visitedDirectory = true

        files.insert(directory, at: 0)

        guard !Task.isCancelled else { return (nil, nil, .cancelled) }
        let (changes, batchElapsed) = await dbManager.perform { [files] dbManager in
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
                keepExistingDownloadState: true,
                preserveDeletedTarget: preserveDeletedTarget
            )
            signposter.endInterval("Depth1BatchWrite", batchWriteState)

            return (changes, batchClock.now - batchStart)
        }

        // The non-paginated depth-1 write (change / working-set full-folder re-read) is the measured
        // enumeration bottleneck: its per-item logical-dedup scans are O(N²) over a flat folder. Log its
        // wall-clock + items/sec so the effect of the normalizedFileName index is visible in the JSONL
        // (the `Depth1BatchWrite` signpost shows the same in Instruments).
        let batchRate = batchElapsed.fpSeconds > 0 ? Double(files.count) / batchElapsed.fpSeconds : 0
        logger.performance(
            "PERF Depth1BatchWrite items=\(files.count) write_s=\(batchElapsed.fpSeconds) items_per_s=\(batchRate)",
            [.url: serverUrl]
        )

        guard let changes else {
            logger.error("Could not persist directory metadata.", [.url: serverUrl])
            return (nil, nil, .metadataDatabaseWriteFailed)
        }

        return (files, changes, nil)
    }

    ///
    /// A depth-1 listing as it is stored after the write, in the server's order.
    ///
    /// The write merges the local flags (`downloaded`, `keepDownloaded`, `visitedDirectory`, `lockToken`) into the rows it persists, while the fresh conversions carry the server defaults. The system must receive the stored state, otherwise it renders pinned or downloaded children as neither. A row which the write left alone, which is a tombstone, which waits at a holding path, or which cannot be read, is handed over as converted.
    ///
    /// The listing itself keeps the server's conversions, because a folder download compares the version it transferred against them.
    ///
    /// - Parameter files: The listing, the directory read first, as ``handleDepth1ReadFileOrFolder(serverUrl:account:dbManager:files:pageIndex:preserveDeletedTarget:log:)`` returns it.
    ///
    static func storedRows(for files: [SendableItemMetadata], dbManager: FilesDatabaseManager) -> [SendableItemMetadata] {
        guard let directory = files.first, directory.directory else {
            return files
        }

        var storedByOcId = Dictionary(
            dbManager.immediateChildItems(directoryMetadata: directory).map { ($0.ocId, $0) },
            uniquingKeysWith: { first, _ in first }
        )

        if let storedDirectory = dbManager.itemMetadata(ocId: directory.ocId) {
            storedByOcId[directory.ocId] = storedDirectory
        }

        return files.map { converted in
            guard let stored = storedByOcId[converted.ocId],
                  !stored.deleted,
                  !stored.isAtHoldingPath
            else {
                return converted
            }

            return stored
        }
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
        expectedDirectoryOcId: String? = nil,
        depth: EnumerateDepth = .targetAndDirectChildren,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void = { _ in },
        isCancelled: @Sendable @escaping () -> Bool = { Task.isCancelled },
        log: any FileProviderLogging
    ) async -> RemoteReadResult {
        guard !isCancelled() else { return RemoteReadResult(error: .cancelled) }
        let logger = FileProviderLogger(category: "Enumerator", log: log)

        guard !serverUrl.contains(FilesDatabaseManager.holdingNameSeparator) else {
            logger.info("Not reading a folder that waits to be placed again by a listing.", [.url: serverUrl])
            return RemoteReadResult(error: .parkedFolderPath)
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

        guard error == .success else {
            logger.error("Read of URL did fail.", [.error: error, .url: serverUrl])
            return RemoteReadResult(error: error)
        }

        guard let data else {
            logger.error("\(depth.rawValue) depth read of url \(serverUrl) did not return data.")
            return RemoteReadResult(error: error)
        }

        guard !isCancelled() else { return RemoteReadResult(error: .cancelled) }
        if let expectedDirectoryOcId {
            // A path may now refer to a different item from the one this download owns.
            let target = files.first { files.isDirectoryToRead($0, directoryToRead: serverUrl) }?.toItemMetadata()
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
            return RemoteReadResult(metadatas: [], nextPage: nextPage)
        }

        func finishTargetRead(_ metadata: SendableItemMetadata, changes: ChangeSet) async -> RemoteReadResult {
            do {
                try await dbManager.addItemMetadataFromRemoteRead(metadata, isCancelled: isCancelled)
            } catch is CancellationError {
                return RemoteReadResult(error: .cancelled)
            } catch {
                // A single-item lookup can still return the server result if caching fails.
                logger.error("Could not persist remote item metadata.", [.item: metadata.ocId, .error: error])
            }
            return RemoteReadResult(metadatas: [metadata], changes: changes, nextPage: nextPage)
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
                let received = receivedFile.toItemMetadata()

                // Read and merge off the cooperative pool: see `FilesDatabaseManager.perform(_:)`.
                let (metadata, newItems, updatedItems) = await dbManager.perform {
                    dbManager -> (SendableItemMetadata, [SendableItemMetadata], [SendableItemMetadata]) in
                    var metadata = received
                    let existing = dbManager.itemMetadata(ocId: metadata.ocId)
                    let isNew = existing == nil
                    let newItems: [SendableItemMetadata] = isNew ? [metadata] : []
                    metadata.lockToken = existing?.lockToken
                    if metadata.etag == existing?.etag {
                        metadata.fileProviderContentVersion = existing?.fileProviderContentVersion
                    }
                    let updatedItems: [SendableItemMetadata] = isNew ? [] : [metadata]
                    metadata.downloaded = existing?.downloaded == true

                    if let existing {
                        metadata.keepDownloaded = existing.keepDownloaded
                    } else {
                        // Genuinely new item: inherit the parent's pin so the
                        // overlay and `contentPolicy` match its siblings (#10054).
                        metadata.keepDownloaded = dbManager.inheritedKeepDownloaded(for: metadata)
                    }

                    return (metadata, newItems, updatedItems)
                }

                return await finishTargetRead(metadata, changes: ChangeSet(created: newItems, updated: updatedItems))
            }
        }

        // A listed folder stored at another path was either moved here or is a second mount of the same folder, which shares its identifier (#10106).
        let listsTarget = depth != .target && !isFollowUpPaginatedRequest
        guard let secondMounts = await secondMountedDirectories(
            among: listsTarget ? Array(files.dropFirst()) : files,
            account: account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            taskHandler: registeringTaskHandler,
            isCancelled: isCancelled,
            logger: logger
        ) else {
            return RemoteReadResult(error: .cancelled)
        }
        files.removeAll { secondMounts[$0.ocId] != nil }

        if depth == .target {
            if let storedMetadata = secondMounts[receivedFile.ocId] {
                return RemoteReadResult(metadatas: [storedMetadata], changes: ChangeSet(), nextPage: nextPage)
            }

            let received = receivedFile.toItemMetadata()

            // Read and merge off the cooperative pool: see
            // `FilesDatabaseManager.perform(_:)`.
            let (metadata, newMetadatas, updatedMetadatas) = await dbManager.perform {
                dbManager -> (SendableItemMetadata, [SendableItemMetadata], [SendableItemMetadata]) in
                var metadata = received
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

                return (metadata, newMetadatas, updatedMetadatas)
            }

            return await finishTargetRead(metadata, changes: ChangeSet(created: newMetadatas, updated: updatedMetadatas))
        } else if depth == .targetAndDirectChildren {
            let depth1Read = await handleDepth1ReadFileOrFolder(
                serverUrl: serverUrl,
                account: account,
                dbManager: dbManager,
                files: files,
                pageIndex: pageSettings?.index,
                preserveDeletedTarget: expectedDirectoryOcId != nil,
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
                guard let stored = storedByOcId[listed.ocId], stored.directory, !stored.deleted, !stored.isTrashed, !stored.isAtHoldingPath,
                      stored.serverUrl.precomposedStringWithCanonicalMapping != listed.serverUrl.precomposedStringWithCanonicalMapping,
                      stored.serverUrl == account.davFilesUrl || manager.parentDirectoryMetadataForItem(stored).map({ !$0.deleted }) == true
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
        remoteInterface: RemoteInterface,
        taskHandler: @Sendable @escaping (URLSessionTask) -> Void = { _ in }
    ) async -> (ocIds: Set<String>, error: NKError) {
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
