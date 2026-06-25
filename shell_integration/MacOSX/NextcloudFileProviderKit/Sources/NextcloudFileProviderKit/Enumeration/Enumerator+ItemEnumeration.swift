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
    func enumerateWorkingSetItems(for observer: NSFileProviderEnumerationObserver) {
        logger.info("Upcoming enumeration is of working set.")
        // Visited folders and downloaded files.
        let materialisedItems = dbManager.materialisedItemMetadatas(account: account.ncKitAccount)
            .filter { !$0.deleted }
        completeEnumerationObserver(observer, nextPage: nil, itemMetadatas: materialisedItems)
    }

    ///
    /// Enumerate the direct children of a regular directory container by reading it from the server,
    /// paginating when the server supports it.
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
            let cursor = paginationCursor(from: page)

            // Check server version to determine if pagination should be enabled.
            // Pagination was fixed in Nextcloud 31 (server bug: https://github.com/nextcloud/server/issues/53674)
            // For older servers, we fall back to non-paginated requests.
            // Note: currentCapabilities uses RetrievedCapabilitiesActor which caches capabilities
            // for 30 minutes, so this call is efficient and doesn't make a network request on every enumeration.
            let (_, capabilities, _, _) = await remoteInterface.currentCapabilities(
                account: account,
                options: .init(),
                taskHandler: { _ in }
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

            logger.info(
                """
                Finished reading page:
                    \(String(data: page.rawValue, encoding: .utf8) ?? "")
                    serverUrl: \(self.serverUrl)
                    for user: \(self.account.ncKitAccount).
                    Processed \(items.count) metadatas
                """
            )

            completeEnumerationObserver(observer, nextPage: nextPage, itemMetadatas: items)
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
    ) -> (index: Int, total: Int?, page: NSFileProviderPage?) {
        guard page != NSFileProviderPage.initialPageSortedByName as NSFileProviderPage,
              page != NSFileProviderPage.initialPageSortedByDate as NSFileProviderPage
        else {
            return (index: 0, total: nil, page: nil)
        }

        guard let response = try? JSONDecoder().decode(EnumeratorPageResponse.self, from: page.rawValue) else {
            logger.error("Could not parse page")
            return (index: 0, total: nil, page: nil)
        }

        return (index: response.index, total: response.total, page: page)
    }
}
