//  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import NextcloudKit

///
/// Full item enumeration (`NSFileProviderEnumerator.enumerateItems`) for the working set and for a
/// regular directory container. The trash container is handled in ``Enumerator`` (Trash) and remote
/// change derivation in ``Enumerator`` (ChangeEnumeration) / (WorkingSetScan).
///
extension Enumerator {
    ///
    /// Enumerate the working set: the items the system keeps track of, i.e. visited folders and
    /// downloaded files, read straight from the local database.
    ///
    func enumerateWorkingSetItems(
        for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage
    ) {
        logger.info("Upcoming enumeration is of working set.")

        // Visited folders and downloaded files, read straight from the local database. The materialised
        // set can be large, so report it one page at a time to stay under the framework's per-page limit.
        // Sort by a stable key (ocId) so the offset cursor stays consistent across the paginated
        // re-invocations the framework drives; the read itself is non-destructive, so re-reading and
        // slicing on each page needs no cross-call buffer.
        guard let storedMaterialisedItems = dbManager.materialisedItemMetadatas(account: account.ncKitAccount) else {
            // Reporting nothing would tell the framework the working set is empty; the framework asks again later.
            logger.error("Could not read the materialized items. Finishing the enumeration with an error.", [.account: account.ncKitAccount])
            observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
            return
        }

        let materialisedItems = storedMaterialisedItems
            .filter { !$0.deleted }
            .sorted { $0.ocId < $1.ocId }

        let offset = Self.itemPageOffset(from: page)
        let pageSize = effectiveBatchSize(suggested: observer.suggestedPageSize)
        let start = min(offset, materialisedItems.count)
        let end = min(start + pageSize, materialisedItems.count)
        let pageItems = Array(materialisedItems[start ..< end])
        let nextPage = end < materialisedItems.count ? Self.itemPage(forOffset: end) : nil

        logger.debug(
            "Enumerating working-set items page. offset: \(offset), pageSize: \(pageSize), pageItems: \(pageItems.count), totalMaterialised: \(materialisedItems.count), hasNextPage: \(nextPage != nil)",
            [.account: account.ncKitAccount]
        )

        completeEnumerationObserver(observer, nextPage: nextPage, itemMetadatas: pageItems)
    }

    ///
    /// Enumerate the direct children of a regular directory container by reading it from the server,
    /// paginating when the server supports it.
    ///
    /// - Note: On servers older than Nextcloud 31 the read is not server-paginated and all direct
    ///   children are reported in one page. A single directory with more than ~100× the suggested page
    ///   size of *direct* children could in theory still hit the framework's per-page limit there. This
    ///   is left unhandled for now: the supported (Nextcloud 31+) path paginates, and the working-set
    ///   item path — the realistic large-listing case — is paged in ``enumerateWorkingSetItems(for:startingAt:)``.
    ///
    func enumerateContainerItems(
        for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage
    ) {
        guard serverUrl != "" else {
            logger.error("Enumerator has empty serverUrl - cannot enumerate that!", [.item: enumeratedItemIdentifier])

            let error = NSError.fileProviderErrorForNonExistentItem(withIdentifier: enumeratedItemIdentifier)
            observer.finishEnumeratingWithError(error)
            return
        }

        logger.debug("Enumerating page: \(String(data: page.rawValue, encoding: .utf8) ?? "")", [.account: account.ncKitAccount, .url: serverUrl])

        Task {
            // Bound the synchronous page work (cursor decode + capabilities + network + convert +
            // persist) for a trace, and record its wall-clock for the JSONL fallback. The `defer` ends
            // the interval on every exit — including the error returns below — so no interval dangles.
            // The trailing observer conversion/report runs in a detached Task and is captured by the
            // separate `ToFileProviderItems` / `ObserverReport` signposts.
            let signposter = EnumerationSignposter.signposter
            let pageWorkState = signposter.beginInterval(
                "EnumeratePageWork",
                id: signposter.makeSignpostID(),
                "serverUrl=\(self.serverUrl, privacy: .public)"
            )
            defer { signposter.endInterval("EnumeratePageWork", pageWorkState) }
            let pageWorkClock = ContinuousClock()
            let pageWorkStart = pageWorkClock.now

            let cursor = paginationCursor(from: page)
            // Rows a page writes carry a fresh `syncTime`, so the last page can tell the children no page returned by this moment. A continuation from a build which did not record it reconciles nothing.
            let listingStart: Date? = cursor.index == 0 ? Date() : cursor.startedAt

            // Check server version to determine if pagination should be enabled.
            // Pagination was fixed in Nextcloud 31 (server bug: https://github.com/nextcloud/server/issues/53674)
            // For older servers, we fall back to non-paginated requests.
            // Note: currentCapabilities uses RetrievedCapabilitiesActor which caches capabilities
            // for 30 minutes, so this call is efficient and doesn't make a network request on every enumeration.
            let (_, capabilities, _, _) = await remoteInterface.currentCapabilities(
                account: account
            )

            let serverMajorVersion = capabilities?.major ?? 0
            let supportsPagination = serverMajorVersion >= 31

            // Enable pagination by passing page settings if the server supports it.
            let pageSettings: (page: NSFileProviderPage?, index: Int, size: Int)? = supportsPagination ? (
                page: cursor.page,
                index: cursor.index,
                size: pageItemCount
            ) : nil

            let readResult = await Self.readServerUrl(
                serverUrl,
                pageSettings: pageSettings,
                account: account,
                remoteInterface: remoteInterface,
                dbManager: dbManager,
                depth: .targetAndDirectChildren,
                log: logger.log
            )

            let items = readResult.metadatas
            let readError = readResult.error
            var nextPage = readResult.nextPage

            guard readError == nil else {
                logger.error("Finishing enumeration for page with error.", [.account: self.account.ncKitAccount, .error: readError, .url: self.serverUrl])

                let error = readError?.fileProviderError(
                    handlingNoSuchItemErrorUsingItemIdentifier: self.enumeratedItemIdentifier
                ) ?? NSFileProviderError(.cannotSynchronize)
                observer.finishEnumeratingWithError(error)
                return
            }

            guard let items else {
                logger.error("Finishing enumeration with invalid metadata.", [.account: self.account.ncKitAccount, .url: self.serverUrl])
                observer.finishEnumeratingWithError(NSFileProviderError(.cannotSynchronize))
                return
            }

            let serverItemTotal = nextPage?.total ?? cursor.total
            if let pendingPage = nextPage, let serverItemTotal, pendingPage.index * pageItemCount >= serverItemTotal {
                // Server will sometimes provide a valid next page data even though there are no
                // items to enumerate anymore
                logger.debug("No more items to enumerate, stopping paged enumeration.")
                nextPage = nil
            }
            // The state the pages come from: the row the first page wrote, or what the token carried.
            let listingState = cursor.index == 0
                ? dbManager.itemMetadata(enumeratedItemIdentifier).map(FilesDatabaseManager.scanRemoteState)
                : cursor.remoteState
            nextPage?.startedAt = listingStart
            nextPage?.remoteState = listingState

            logger.info(
                """
                Finished reading page:
                    \(String(data: page.rawValue, encoding: .utf8) ?? "")
                    serverUrl: \(self.serverUrl)
                    for user: \(self.account.ncKitAccount).
                    Processed \(items.count) metadatas
                """
            )

            // completeEnumerationObserver finishes on an already-encoded page, so serialise the server's
            // pagination cursor here (nil ends the enumeration). Each server page is already bounded by
            // pageItemCount, so it stays under the framework's per-page limit.
            let rawNextPage: NSFileProviderPage?
            if let nextPage, let nextPageData = try? JSONEncoder().encode(nextPage) {
                logger.info("Next page: \(String(data: nextPageData, encoding: .utf8) ?? "?")")
                rawNextPage = NSFileProviderPage(nextPageData)
            } else {
                rawNextPage = nil
            }

            let pageWorkElapsed = pageWorkClock.now - pageWorkStart
            logger.performance(
                "PERF EnumeratePageWork items=\(items.count) page_work_s=\(pageWorkElapsed.fpSeconds) hasNextPage=\(rawNextPage != nil)",
                [.url: self.serverUrl]
            )

            // A few milliseconds of store work before the last page is handed over, so the observer's finish means the store is reconciled. Only a listing which compared the children with the stored rows reconciles: the depth-1 write did, a paginated listing only once its by-date drop ran.
            // The server saying that no page follows is what ends the listing here: a page which could not be encoded ends the enumeration too, but then nothing was compared.
            if nextPage == nil {
                if pageSettings == nil {
                    reconcileEnumeratedDirectory(stateAtListingStart: listingState)
                } else if let listingStart {
                    dropChildrenNotListed(since: listingStart)
                    reconcileEnumeratedDirectory(stateAtListingStart: listingState)
                }
            }

            // A paged write returns the rows as stored; a depth-1 one returns the server's listing, so
            // the local state the write merged in is read back before the page reaches the system.
            let handedOver = pageSettings == nil
                ? await dbManager.perform { Self.storedRows(for: items, dbManager: $0) }
                : items

            completeEnumerationObserver(observer, nextPage: rawNextPage, itemMetadatas: handedOver)
        }
    }

    ///
    /// Decode the Nextcloud pagination cursor carried by an `NSFileProviderPage`.
    ///
    /// The framework's default initial pages are not valid Nextcloud pagination tokens, so they —
    /// and any page that fails to decode — map to a fresh first-page cursor `(index: 0, total: nil,
    /// page: nil)`. A decoded continuation returns its stored index and total alongside the original
    /// page, which the server needs as the pagination token.
    ///
    private func paginationCursor(
        from page: NSFileProviderPage
    ) -> (index: Int, total: Int?, page: NSFileProviderPage?, startedAt: Date?, remoteState: String?) {
        guard page != NSFileProviderPage.initialPageSortedByName as NSFileProviderPage,
              page != NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage
        else {
            return (index: 0, total: nil, page: nil, startedAt: nil, remoteState: nil)
        }

        guard let response = try? JSONDecoder().decode(EnumeratorPageResponse.self, from: page.rawValue) else {
            logger.error("Could not parse page")
            return (index: 0, total: nil, page: nil, startedAt: nil, remoteState: nil)
        }

        return (index: response.index, total: response.total, page: page, startedAt: response.startedAt, remoteState: response.remoteState)
    }

    ///
    /// Record the enumerated directory as reconciled for the working-set scan when nothing below it is left for the scan to descend into.
    ///
    /// The listing the system asked for compared the directory's children with the stored rows, like a scan listing does. Deeper levels are covered by their own state and by the server's etag propagation, so the scan can skip the directory until its etag moves. Without this, every directory the system visits stays unreconciled until a change under its parent makes a pass list all of them at once.
    ///
    /// Only the directory listed is recorded, never an ancestor: an ancestor's stored etag may have been refreshed by a listing of its own parent without its children having been compared, and recording it would hide a change below it for good. Left to the scan as well: the root container, which is listed on every pass; a mount point, whose state the scan keeps as the parent lists it, with the permissions the server rewrites there; a directory whose stored state moved on while its pages were read; and a directory with a child the scan would still descend into, that is a child which is visited or holds something materialised and whose state differs from its own reconciled state, or which has an external storage at or below it.
    ///
    /// - Parameter stateAtListingStart: The directory's state as the listing's first page reported it; `nil` for a continuation from a build which did not record it.
    ///
    private func reconcileEnumeratedDirectory(stateAtListingStart: String?) {
        guard !enumeratingSystemIdentifier,
              let directory = dbManager.itemMetadata(enumeratedItemIdentifier), directory.directory, !directory.deleted,
              let parent = storedParent(of: directory)
        else {
            return
        }

        guard let stateAtListingStart, FilesDatabaseManager.scanRemoteState(of: directory) == stateAtListingStart else {
            logger.debug("Leaving a directory whose stored state moved on while its pages were read to the working-set scan.", [.item: directory.ocId, .url: serverUrl])
            return
        }

        reconcile(directory, listedBy: parent)
    }

    private func storedParent(of directory: SendableItemMetadata) -> SendableItemMetadata? {
        // The root container row is not addressed by its URL.
        if directory.serverUrl == account.davFilesUrl {
            return dbManager.itemMetadata(ocId: NSFileProviderItemIdentifier.rootContainer.rawValue)
        }

        return dbManager.itemMetadata(account: account.ncKitAccount, locatedAtRemoteUrl: directory.serverUrl)
    }

    private func reconcile(_ directory: SendableItemMetadata, listedBy parent: SendableItemMetadata) {
        guard parent.mountType == directory.mountType else {
            logger.debug("Leaving a mount point to the working-set scan.", [.item: directory.ocId, .url: directory.remotePath()])
            return
        }

        let children = dbManager.immediateChildItems(directoryMetadata: directory).filter { $0.directory && !$0.deleted && !isLockFileName($0.fileName) }

        guard let reconciledStates = dbManager.scannedRemoteStates(ocIds: children.map(\.ocId)) else {
            return
        }

        for child in children {
            let childPath = child.remotePath()
            let decision = ScanDescentDecision(
                child: child,
                holdsMaterialisedItem: dbManager.hasMaterialisedItem(below: childPath) ?? true,
                holdsExternalStorage: dbManager.hasExternalStorage(below: childPath) ?? true,
                reconciledState: reconciledStates[child.ocId]
            )

            if decision == .descend {
                logger.debug("Leaving a directory with a child the working-set scan has to list to the scan.", [.item: directory.ocId, .url: childPath])
                return
            }
        }

        dbManager.recordScannedRemoteStates([directory.ocId: FilesDatabaseManager.scanRemoteState(of: directory)], forgetting: [])
    }

    ///
    /// Tombstone the stored children a complete paginated listing did not return.
    ///
    /// A paginated listing writes each page as it arrives and cannot compare a page with the stored rows, unlike the depth-1 write of the other paths. The system drops the items the listing did not return from the container, and so does the store now, which otherwise would keep them as live, materialised rows.
    ///
    private func dropChildrenNotListed(since listingStart: Date) {
        guard let dropped = dbManager.deleteChildrenNotListed(of: serverUrl, account: account.ncKitAccount, since: listingStart) else {
            return
        }

        if !dropped.isEmpty {
            logger.info("Tombstoned the children a paginated listing no longer returned.", [.url: serverUrl, .name: "\(dropped.count)"])
        }
    }

    ///
    /// The offset a client-paginated item enumeration should resume from.
    ///
    /// Used by the item paths that read the whole listing into memory and page over it locally (the
    /// working set; see ``enumerateWorkingSetItems(for:startingAt:)``). The cursor is the decimal offset
    /// encoded as UTF-8 — tiny (well under the 500-byte page limit) and never failing to encode. The
    /// framework's opaque initial pages, and anything that is not a non-negative decimal, map to offset `0`.
    ///
    static func itemPageOffset(from page: NSFileProviderPage) -> Int {
        guard page != NSFileProviderPage.initialPageSortedByName as NSFileProviderPage,
              page != NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage,
              let text = String(data: page.rawValue, encoding: .utf8),
              let offset = Int(text), offset >= 0
        else {
            return 0
        }
        return offset
    }

    ///
    /// Encode a continuation page for a client-paginated item enumeration. Encoding a decimal offset
    /// cannot fail, so this never silently truncates the enumeration.
    ///
    static func itemPage(forOffset offset: Int) -> NSFileProviderPage {
        NSFileProviderPage(Data(String(offset).utf8))
    }
}
