//
//  ShareTableViewDataSource.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 27/2/24.
//

import AppKit
import FileProvider
import NextcloudKit
import OSLog

class ShareTableViewDataSource: NSObject, NSTableViewDataSource, NSTableViewDelegate {
    private let shareItemViewIdentifier = NSUserInterfaceItemIdentifier("ShareTableItemView")
    private let shareItemViewNib = NSNib(nibNamed: "ShareTableItemView", bundle: nil)

    var uiDelegate: ShareViewDataSourceUIDelegate?
    var sharesTableView: NSTableView? {
        didSet {
            sharesTableView?.register(shareItemViewNib, forIdentifier: shareItemViewIdentifier)
            sharesTableView?.rowHeight = 42.0  // Height of view in ShareTableItemView XIB
            sharesTableView?.dataSource = self
            sharesTableView?.delegate = self
            sharesTableView?.reloadData()
        }
    }
    var shareCapabilities = ShareCapabilities()
    var itemMetadata: NKFile?

    private(set) var kit: NextcloudKit?
    private(set) var itemURL: URL?
    private(set) var itemServerRelativePath: String?
    private var shares: [NKShare] = [] {
        didSet { Task { @MainActor in sharesTableView?.reloadData() } }
    }
    private var account: NextcloudAccount? {
        didSet {
            guard let account = account else { return }
            kit = NextcloudKit()
            kit?.setup(
                user: account.username,
                userId: account.username,
                password: account.password,
                urlBase: account.serverUrl
            )
        }
    }

    func loadItem(url: URL) {
        itemServerRelativePath = nil
        itemURL = url
        Task {
            await reload()
        }
    }

    func reload() async {
        guard let itemURL = itemURL else { return }
        guard let itemIdentifier = await withCheckedContinuation({
            (continuation: CheckedContinuation<NSFileProviderItemIdentifier?, Never>) -> Void in
            NSFileProviderManager.getIdentifierForUserVisibleFile(
                at: itemURL
            ) { identifier, domainIdentifier, error in
                defer { continuation.resume(returning: identifier) }
                guard error == nil else {
                    Logger.sharesDataSource.error("No identifier: \(error, privacy: .public)")
                    return
                }
            }
        }) else {
            Logger.sharesDataSource.error("Could not get identifier for item, no shares.")
            return
        }

        do {
            let connection = try await serviceConnection(url: itemURL)
            guard let serverPath = await connection.itemServerPath(identifier: itemIdentifier),
                  let credentials = await connection.credentials() as? Dictionary<String, String>,
                  let convertedAccount = NextcloudAccount(dictionary: credentials) else {
                Logger.sharesDataSource.error("Failed to get details from FileProviderExt")
                return
            }
            let serverPathString = serverPath as String
            itemServerRelativePath = serverPathString
            account = convertedAccount
            await sharesTableView?.deselectAll(self)
            shareCapabilities = await fetchCapabilities()
            guard shareCapabilities.apiEnabled else {
                let errorMsg = "Server does not support shares."
                Logger.sharesDataSource.info("\(errorMsg)")
                uiDelegate?.showError(errorMsg)
                return
            }
            itemMetadata = await fetchItemMetadata(itemRelativePath: serverPathString)
            guard itemMetadata?.permissions.contains("R") == true else {
                let errorMsg = "This file cannot be shared."
                Logger.sharesDataSource.warning("\(errorMsg)")
                uiDelegate?.showError(errorMsg)
                return
            }
            shares = await fetch(
                itemIdentifier: itemIdentifier, itemRelativePath: serverPathString
            )
        } catch let error {
            Logger.sharesDataSource.error("Could not reload data: \(error, privacy: .public)")
        }
    }

    private func serviceConnection(url: URL) async throws -> FPUIExtensionService {
        let services = try await FileManager().fileProviderServicesForItem(at: url)
        guard let service = services[fpUiExtensionServiceName] else {
            Logger.sharesDataSource.error("Couldn't get service, required service not present")
            throw NSFileProviderError(.providerNotFound)
        }
        let connection: NSXPCConnection
        connection = try await service.fileProviderConnection()
        connection.remoteObjectInterface = NSXPCInterface(with: FPUIExtensionService.self)
        connection.interruptionHandler = {
            Logger.sharesDataSource.error("Service connection interrupted")
        }
        connection.resume()
        guard let proxy = connection.remoteObjectProxy as? FPUIExtensionService else {
            throw NSFileProviderError(.serverUnreachable)
        }
        return proxy
    }

    private func fetch(
        itemIdentifier: NSFileProviderItemIdentifier, itemRelativePath: String
    ) async -> [NKShare] {
        Task { @MainActor in uiDelegate?.fetchStarted() }
        defer { Task { @MainActor in uiDelegate?.fetchFinished() } }

        let rawIdentifier = itemIdentifier.rawValue
        Logger.sharesDataSource.info("Fetching shares for item \(rawIdentifier, privacy: .public)")

        guard let kit = kit else {
            Logger.sharesDataSource.error("NextcloudKit instance is nil")
            return []
        }

        let parameter = NKShareParameter(path: itemRelativePath)

        return await withCheckedContinuation { continuation in
            kit.readShares(parameters: parameter) { account, shares, data, error in
                let shareCount = shares?.count ?? 0
                Logger.sharesDataSource.info("Received \(shareCount, privacy: .public) shares")
                defer { continuation.resume(returning: shares ?? []) }
                guard error == .success else {
                    let errorString = "Error fetching shares: \(error.errorDescription)"
                    Logger.sharesDataSource.error("\(errorString)")
                    Task { @MainActor in self.uiDelegate?.showError(errorString) }
                    return
                }
            }
        }
    }

    private func fetchCapabilities() async -> ShareCapabilities {
        return await withCheckedContinuation { continuation in
            kit?.getCapabilities { account, capabilitiesJson, error in
                guard error == .success, let capabilitiesJson = capabilitiesJson else {
                    let errorString = "Error getting server capabilities: \(error.errorDescription)"
                    Logger.sharesDataSource.error("\(errorString, privacy: .public)")
                    Task { @MainActor in self.uiDelegate?.showError(errorString) }
                    continuation.resume(returning: ShareCapabilities())
                    return
                }
                Logger.sharesDataSource.info("Successfully retrieved server share capabilities")
                continuation.resume(returning: ShareCapabilities(json: capabilitiesJson))
            }
        }
    }

    private func fetchItemMetadata(itemRelativePath: String) async -> NKFile? {
        guard let kit = kit else {
            let errorString = "Could not fetch item metadata as nckit unavailable"
            Logger.sharesDataSource.error("\(errorString, privacy: .public)")
            Task { @MainActor in self.uiDelegate?.showError(errorString) }
            return nil
        }

        func slashlessPath(_ string: String) -> String {
            var strCopy = string
            if strCopy.hasPrefix("/") {
                strCopy.removeFirst()
            }
            if strCopy.hasSuffix("/") {
                strCopy.removeLast()
            }
            return strCopy
        }

        let nkCommon = kit.nkCommonInstance
        let urlBase = slashlessPath(nkCommon.urlBase)
        let davSuffix = slashlessPath(nkCommon.dav)
        let userId = nkCommon.userId
        let itemRelPath = slashlessPath(itemRelativePath)

        let itemFullServerPath = "\(urlBase)/\(davSuffix)/files/\(userId)/\(itemRelPath)"
        return await withCheckedContinuation { continuation in
            kit.readFileOrFolder(serverUrlFileName: itemFullServerPath, depth: "0") {
                account, files, data, error in
                guard error == .success else {
                    let errorString = "Error getting item metadata: \(error.errorDescription)"
                    Logger.sharesDataSource.error("\(errorString, privacy: .public)")
                    Task { @MainActor in self.uiDelegate?.showError(errorString) }
                    continuation.resume(returning: nil)
                    return
                }
                Logger.sharesDataSource.info("Successfully retrieved item metadata")
                continuation.resume(returning: files.first)
            }
        }
    }

    // MARK: - NSTableViewDataSource protocol methods

    @objc func numberOfRows(in tableView: NSTableView) -> Int {
        shares.count
    }

    // MARK: - NSTableViewDelegate protocol methods

    @objc func tableView(
        _ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int
    ) -> NSView? {
        let share = shares[row]
        guard let view = tableView.makeView(
            withIdentifier: shareItemViewIdentifier, owner: self
        ) as? ShareTableItemView else {
            Logger.sharesDataSource.error("Acquired item view from table is not a Share item view!")
            return nil
        }
        view.share = share
        return view
    }

    @objc func tableViewSelectionDidChange(_ notification: Notification) {
        guard let selectedRow = sharesTableView?.selectedRow, selectedRow >= 0 else {
            Task { @MainActor in uiDelegate?.hideOptions() }
            return
        }
        let share = shares[selectedRow]
        Task { @MainActor in uiDelegate?.showOptions(share: share) }
    }
}
