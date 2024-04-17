/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import FileProvider
import NCDesktopClientSocketKit
import NextcloudKit
import NextcloudFileProviderKit
import OSLog

@objc class FileProviderExtension: NSObject, NSFileProviderReplicatedExtension, NKCommonDelegate {
    let domain: NSFileProviderDomain
    let ncKit = NextcloudKit()
    let appGroupIdentifier = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix") as? String
    var ncAccount: Account?
    var changeObserver: RemoteChangeObserver?
    lazy var ncKitBackground = NKBackground(nkCommonInstance: ncKit.nkCommonInstance)
    lazy var socketClient: LocalSocketClient? = {
        guard let containerUrl = pathForAppGroupContainer() else {
            Logger.fileProviderExtension.critical("Won't start socket client, no container url")
            return nil;
        }

        let socketPath = containerUrl.appendingPathComponent(
            ".fileprovidersocket", conformingTo: .archive)
        let lineProcessor = FileProviderSocketLineProcessor(delegate: self)
        return LocalSocketClient(socketPath: socketPath.path, lineProcessor: lineProcessor)
    }()

    let urlSessionIdentifier = "com.nextcloud.session.upload.fileproviderext"
    let urlSessionMaximumConnectionsPerHost = 5
    lazy var urlSession: URLSession = {
        let configuration = URLSessionConfiguration.background(withIdentifier: urlSessionIdentifier)
        configuration.allowsCellularAccess = true
        configuration.sessionSendsLaunchEvents = true
        configuration.isDiscretionary = false
        configuration.httpMaximumConnectionsPerHost = urlSessionMaximumConnectionsPerHost
        configuration.requestCachePolicy = NSURLRequest.CachePolicy.reloadIgnoringLocalCacheData
        configuration.sharedContainerIdentifier = appGroupIdentifier

        let session = URLSession(
            configuration: configuration,
            delegate: ncKitBackground,
            delegateQueue: OperationQueue.main
        )
        return session
    }()

    // Whether or not we are going to recursively scan new folders when they are discovered.
    // Apple's recommendation is that we should always scan the file hierarchy fully.
    // This does lead to long load times when a file provider domain is initially configured.
    // We can instead do a fast enumeration where we only scan folders as the user navigates through
    // them, thereby avoiding this issue; the trade-off is that we will be unable to detect
    // materialised file moves to unexplored folders, therefore deleting the item when we could have
    // just moved it instead.
    //
    // Since it's not desirable to cancel a long recursive enumeration half-way through, we do the
    // fast enumeration by default. We prompt the user on the client side to run a proper, full
    // enumeration if they want for safety.
    lazy var config = FileProviderConfig(domainIdentifier: domain.identifier)

    required init(domain: NSFileProviderDomain) {
        // The containing application must create a domain using 
        // `NSFileProviderManager.add(_:, completionHandler:)`. The system will then launch the
        // application extension process, call `FileProviderExtension.init(domain:)` to instantiate
        // the extension for that domain, and call methods on the instance.
        self.domain = domain
        super.init()
        socketClient?.start()
    }

    func invalidate() {
        // TODO: cleanup any resources
        Logger.fileProviderExtension.debug(
            "Extension for domain \(self.domain.displayName, privacy: .public) is being torn down"
        )
    }

    // MARK: - NSFileProviderReplicatedExtension protocol methods

    func item(
        for identifier: NSFileProviderItemIdentifier, 
        request _: NSFileProviderRequest,
        completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void
    ) -> Progress {
        if let item = Item.storedItem(identifier: identifier, usingKit: ncKit) {
            completionHandler(item, nil)
        } else {
            completionHandler(nil, NSFileProviderError(.noSuchItem))
        }
        return Progress()
    }

    func fetchContents(
        for itemIdentifier: NSFileProviderItemIdentifier,
        version requestedVersion: NSFileProviderItemVersion?, 
        request: NSFileProviderRequest,
        completionHandler: @escaping (URL?, NSFileProviderItem?, Error?) -> Void
    ) -> Progress {
        Logger.fileProviderExtension.debug(
            "Received request to fetch contents of item with identifier: \(itemIdentifier.rawValue, privacy: .public)"
        )

        guard requestedVersion == nil else {
            // TODO: Add proper support for file versioning
            Logger.fileProviderExtension.error(
                "Can't return contents for a specific version as this is not supported."
            )
            completionHandler(
                nil, 
                nil,
                NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError)
            )
            return Progress()
        }

        guard ncAccount != nil else {
            Logger.fileProviderExtension.error(
                """
                Not fetching contents for item: \(itemIdentifier.rawValue, privacy: .public)
                as account not set up yet.
                """
            )
            completionHandler(nil, nil, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let item = Item.storedItem(identifier: itemIdentifier, usingKit: ncKit) else {
            Logger.fileProviderExtension.error(
                """
                Not fetching contents for item: \(itemIdentifier.rawValue, privacy: .public)
                as item not found.
                """
            )
            completionHandler(nil, nil, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        let progress = Progress()
        Task {
            let (localUrl, updatedItem, error) = await item.fetchContents(
                domain: self.domain, progress: progress
            )
            completionHandler(localUrl, updatedItem, error)
        }
        return progress
    }

    func createItem(
        basedOn itemTemplate: NSFileProviderItem, 
        fields: NSFileProviderItemFields,
        contents url: URL?, 
        options: NSFileProviderCreateItemOptions = [],
        request: NSFileProviderRequest,
        completionHandler: @escaping ( 
            NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?
        ) -> Void
    ) -> Progress {

        let tempId = itemTemplate.itemIdentifier.rawValue
        Logger.fileProviderExtension.debug(
            """
            Received create item request for item with identifier: \(tempId, privacy: .public)
            and filename: \(itemTemplate.filename, privacy: .public)
            """
        )

        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                """
                Not creating item: \(itemTemplate.itemIdentifier.rawValue, privacy: .public)
                as account not set up yet
                """
            )
            completionHandler(
                itemTemplate, 
                NSFileProviderItemFields(),
                false,
                NSFileProviderError(.notAuthenticated)
            )
            return Progress()
        }

        let progress = Progress()
        Task {
            let (item, error) = await Item.create(
                basedOn: itemTemplate,
                fields: fields,
                contents: url,
                request: request,
                domain: self.domain,
                ncKit: ncKit,
                ncAccount: ncAccount,
                progress: progress
            )
            if error != nil {
                signalEnumerator(completionHandler: { _ in })
            }
            completionHandler(
                item ?? itemTemplate,
                NSFileProviderItemFields(),
                false,
                error
            )
        }
        return progress
    }

    func modifyItem(
        _ item: NSFileProviderItem, 
        baseVersion: NSFileProviderItemVersion,
        changedFields: NSFileProviderItemFields, 
        contents newContents: URL?,
        options: NSFileProviderModifyItemOptions = [], 
        request: NSFileProviderRequest,
        completionHandler: @escaping (
            NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?
        ) -> Void
    ) -> Progress {
        // An item was modified on disk, process the item's modification
        // TODO: Handle finder things like tags, other possible item changed fields

        let identifier = item.itemIdentifier
        let ocId = identifier.rawValue
        Logger.fileProviderExtension.debug(
            """
            Received modify item request for item with identifier: \(ocId, privacy: .public)
            and filename: \(item.filename, privacy: .public)
            """
        )

        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not modifying item: \(ocId, privacy: .public) as account not set up yet."
            )
            completionHandler(item, [], false, NSFileProviderError(.notAuthenticated))
            return Progress()
        }

        guard let existingItem = Item.storedItem(identifier: identifier, usingKit: ncKit) else {
            Logger.fileProviderExtension.error(
                "Not modifying item: \(ocId, privacy: .public) as item not found."
            )
            completionHandler(item, [], false, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        let progress = Progress()
        Task {
            let (modifiedItem, error) = await existingItem.modify(
                itemTarget: item,
                baseVersion: baseVersion,
                changedFields: changedFields,
                contents: newContents,
                options: options,
                request: request,
                ncAccount: ncAccount,
                domain: domain,
                progress: progress
            )
            if error != nil {
                signalEnumerator(completionHandler: { _ in })
            }
            completionHandler(modifiedItem ?? item, [], false, error)
        }
        return progress
    }

    func deleteItem(
        identifier: NSFileProviderItemIdentifier, 
        baseVersion _: NSFileProviderItemVersion,
        options _: NSFileProviderDeleteItemOptions = [], 
        request _: NSFileProviderRequest,
        completionHandler: @escaping (Error?) -> Void
    ) -> Progress {
        Logger.fileProviderExtension.debug(
            "Received delete request for item: \(identifier.rawValue, privacy: .public)"
        )

        guard ncAccount != nil else {
            Logger.fileProviderExtension.error(
                "Not deleting item \(identifier.rawValue, privacy: .public), account not set up yet"
            )
            completionHandler(NSFileProviderError(.notAuthenticated))
            return Progress()
        }


        guard let item = Item.storedItem(identifier: identifier, usingKit: ncKit) else {
            Logger.fileProviderExtension.error(
                "Not deleting item \(identifier.rawValue, privacy: .public), item not found"
            )
            completionHandler(NSFileProviderError(.noSuchItem))
            return Progress()
        }

        let progress = Progress(totalUnitCount: 1)
        Task {
            let error = await item.delete()
            if error != nil {
                signalEnumerator(completionHandler: { _ in })
            }
            progress.completedUnitCount = 1
            completionHandler(await item.delete())
        }
        return progress
    }

    func enumerator(
        for containerItemIdentifier: NSFileProviderItemIdentifier, request _: NSFileProviderRequest
    ) throws -> NSFileProviderEnumerator {
        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not providing enumerator for container with identifier \(containerItemIdentifier.rawValue, privacy: .public) yet as account not set up"
            )
            throw NSFileProviderError(.notAuthenticated)
        }

        return Enumerator(
            enumeratedItemIdentifier: containerItemIdentifier,
            ncAccount: ncAccount,
            ncKit: ncKit,
            domain: domain,
            fastEnumeration: config.fastEnumerationEnabled
        )
    }

    func materializedItemsDidChange(completionHandler: @escaping () -> Void) {
        guard let ncAccount else {
            Logger.fileProviderExtension.error(
                "Not purging stale local file metadatas, account not set up")
            completionHandler()
            return
        }

        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error(
                "Could not get file provider manager for domain: \(self.domain.displayName, privacy: .public)"
            )
            completionHandler()
            return
        }

        let materialisedEnumerator = fpManager.enumeratorForMaterializedItems()
        let materialisedObserver = MaterialisedEnumerationObserver(
            ncKitAccount: ncAccount.ncKitAccount
        ) { _ in
            completionHandler()
        }
        let startingPage = NSFileProviderPage(NSFileProviderPage.initialPageSortedByName as Data)

        materialisedEnumerator.enumerateItems(for: materialisedObserver, startingAt: startingPage)
    }

    func signalEnumerator(completionHandler: @escaping (_ error: Error?) -> Void) {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            Logger.fileProviderExtension.error(
                "Could not get file provider manager for domain, could not signal enumerator. This might lead to future conflicts."
            )
            return
        }

        fpManager.signalEnumerator(for: .workingSet, completionHandler: completionHandler)
    }
}
