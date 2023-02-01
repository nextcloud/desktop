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
import OSLog
import NCDesktopClientSocketKit
import NextcloudKit

class FileProviderExtension: NSObject, NSFileProviderReplicatedExtension, NKCommonDelegate {
    let domain: NSFileProviderDomain

    let appGroupIdentifier: String? = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix") as? String
    var ncAccount: NextcloudAccount?
    lazy var socketClient: LocalSocketClient? = {
        guard let containerUrl = pathForAppGroupContainer() else {
            NSLog("Could not start file provider socket client properly as could not get container url")
            return nil;
        }

        let socketPath = containerUrl.appendingPathComponent(".fileprovidersocket", conformingTo: .archive)
        let lineProcessor = FileProviderSocketLineProcessor(delegate: self)

        return LocalSocketClient(socketPath: socketPath.path, lineProcessor: lineProcessor)
    }()

    let urlSessionIdentifier: String = "com.nextcloud.session.upload.fileproviderext"
    let urlSessionMaximumConnectionsPerHost = 5
    lazy var urlSession: URLSession = {
        let configuration = URLSessionConfiguration.background(withIdentifier: urlSessionIdentifier)
        configuration.allowsCellularAccess = true
        configuration.sessionSendsLaunchEvents = true
        configuration.isDiscretionary = false
        configuration.httpMaximumConnectionsPerHost = urlSessionMaximumConnectionsPerHost
        configuration.requestCachePolicy = NSURLRequest.CachePolicy.reloadIgnoringLocalCacheData
        configuration.sharedContainerIdentifier = appGroupIdentifier
        let session = URLSession(configuration: configuration, delegate: NKBackground.shared, delegateQueue: OperationQueue.main)
        return session
    }()

    private var itemIdsForEnumeratorsNeedingSignalling: NSMutableSet = NSMutableSet()

    required init(domain: NSFileProviderDomain) {
        self.domain = domain
        // The containing application must create a domain using `NSFileProviderManager.add(_:, completionHandler:)`. The system will then launch the application extension process, call `FileProviderExtension.init(domain:)` to instantiate the extension for that domain, and call methods on the instance.

        super.init()
        self.socketClient?.start()
    }
    
    func invalidate() {
        // TODO: cleanup any resources
    }

    // MARK: NSFileProviderReplicatedExtension protocol methods
    
    func item(for identifier: NSFileProviderItemIdentifier, request: NSFileProviderRequest, completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void) -> Progress {
        // resolve the given identifier to a record in the model

        NSLog("Received item request for item with identifier: %@", identifier.rawValue)
        if identifier == .rootContainer {
            guard let ncAccount = ncAccount else {
                NSLog("Not providing item: %@ as account not set up yet", identifier.rawValue)
                completionHandler(nil, NSFileProviderError(.notAuthenticated))
                return Progress()
            }

            let metadata = NextcloudItemMetadataTable()

            metadata.account = ncAccount.ncKitAccount
            metadata.directory = true
            metadata.ocId = NSFileProviderItemIdentifier.rootContainer.rawValue
            metadata.fileName = "root"
            metadata.fileNameView = "root"
            metadata.serverUrl = ncAccount.serverUrl
            metadata.classFile = NKCommon.typeClassFile.directory.rawValue

            completionHandler(FileProviderItem(metadata: metadata, parentItemIdentifier: NSFileProviderItemIdentifier.rootContainer), nil)
            return Progress()
        }

        let dbManager = NextcloudFilesDatabaseManager.shared
        guard let metadata = dbManager.itemMetadataFromFileProviderItemIdentifier(identifier),
              let parentItemIdentifier = parentItemIdentifierFromMetadata(metadata) else {
            completionHandler(nil, NSFileProviderError(.noSuchItem))
            return Progress()
        }

        completionHandler(FileProviderItem(metadata: metadata, parentItemIdentifier: parentItemIdentifier), nil)
        return Progress()
    }
    
    func fetchContents(for itemIdentifier: NSFileProviderItemIdentifier, version requestedVersion: NSFileProviderItemVersion?, request: NSFileProviderRequest, completionHandler: @escaping (URL?, NSFileProviderItem?, Error?) -> Void) -> Progress {
        // TODO: implement fetching of the contents for the itemIdentifier at the specified version
        
        completionHandler(nil, nil, NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:]))
        return Progress()
    }
    
    func createItem(basedOn itemTemplate: NSFileProviderItem, fields: NSFileProviderItemFields, contents url: URL?, options: NSFileProviderCreateItemOptions = [], request: NSFileProviderRequest, completionHandler: @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?) -> Void) -> Progress {
        // TODO: a new item was created on disk, process the item's creation
        
        completionHandler(itemTemplate, [], false, nil)
        return Progress()
    }
    
    func modifyItem(_ item: NSFileProviderItem, baseVersion version: NSFileProviderItemVersion, changedFields: NSFileProviderItemFields, contents newContents: URL?, options: NSFileProviderModifyItemOptions = [], request: NSFileProviderRequest, completionHandler: @escaping (NSFileProviderItem?, NSFileProviderItemFields, Bool, Error?) -> Void) -> Progress {
        // TODO: an item was modified on disk, process the item's modification
        
        completionHandler(nil, [], false, NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:]))
        return Progress()
    }
    
    func deleteItem(identifier: NSFileProviderItemIdentifier, baseVersion version: NSFileProviderItemVersion, options: NSFileProviderDeleteItemOptions = [], request: NSFileProviderRequest, completionHandler: @escaping (Error?) -> Void) -> Progress {
        // TODO: an item was deleted on disk, process the item's deletion
        
        completionHandler(NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:]))
        return Progress()
    }
    
    func enumerator(for containerItemIdentifier: NSFileProviderItemIdentifier, request: NSFileProviderRequest) throws -> NSFileProviderEnumerator {

        guard let ncAccount = ncAccount else {
            NSLog("Not providing enumerator for container with identifier %@ yet as account not set up")
            itemIdsForEnumeratorsNeedingSignalling.add(containerItemIdentifier)
            throw NSFileProviderError(.notAuthenticated)
        }

        return FileProviderEnumerator(enumeratedItemIdentifier: containerItemIdentifier, ncAccount: ncAccount)
    }

    // MARK: Nextcloud desktop client communication
    func sendFileProviderDomainIdentifier() {
        let command = "FILE_PROVIDER_DOMAIN_IDENTIFIER_REQUEST_REPLY"
        let argument = domain.identifier.rawValue
        let message = command + ":" + argument + "\n"
        socketClient?.sendMessage(message)
    }

    private func signalEnumeratorAfterAccountSetup() {
        guard let fpManager = NSFileProviderManager(for: domain) else {
            NSLog("Could not get file provider manager for domain %@, cannot notify after account setup", domain)
            return
        }

        assert(ncAccount != nil)

        if !NextcloudFilesDatabaseManager.shared.anyItemMetadatasForAccount(ncAccount!.ncKitAccount) {
            // This refreshes the entire structure of the FileProvider and calls
            // enumerateItems rather than enumerateChanges in the enumerator
            NSLog("Signalling manager for user %@ at server %@ to reimport everything", ncAccount!.username, ncAccount!.serverUrl)
            fpManager.reimportItems(below: .rootContainer, completionHandler: {_ in })
            return
        }

        NSLog("Signalling enumerator for user %@ at server %@", ncAccount!.username, ncAccount!.serverUrl)
        // System will only respond to workingSet when using and NSFileProviderReplicatedExtension
        // https://developer.apple.com/documentation/fileprovider/nonreplicated_file_provider_extension/content_and_change_tracking/tracking_your_file_provider_s_changes/using_push_notifications_to_signal_changes
        fpManager.signalEnumerator(for: .workingSet) { error in
            if error != nil {
                NSLog("Error signalling enumerator for workingSet, received error: %@", error!.localizedDescription)
            }
        }
    }

    func setupDomainAccount(user: String, serverUrl: String, password: String) {
        ncAccount = NextcloudAccount(user: user, serverUrl: serverUrl, password: password)

        NextcloudKit.shared.setup(user: ncAccount!.username,
                                  userId: ncAccount!.username,
                                  password: ncAccount!.password,
                                  urlBase: ncAccount!.serverUrl,
                                  userAgent: "Nextcloud-macOS/FileProviderExt",
                                  nextcloudVersion: 25,
                                  delegate: nil) // TODO: add delegate methods for self

        NSLog("Nextcloud account set up in File Provider extension for user: %@ at server: %@", user, serverUrl)

        if itemIdsForEnumeratorsNeedingSignalling.count > 0 {
            signalEnumeratorAfterAccountSetup()
            itemIdsForEnumeratorsNeedingSignalling = NSMutableSet()
        }
    }
}
