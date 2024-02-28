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

    var sharesTableView: NSTableView? {
        didSet {
            sharesTableView?.register(shareItemViewNib, forIdentifier: shareItemViewIdentifier)
            sharesTableView?.rowHeight = 42.0  // Height of view in ShareTableItemView XIB
            sharesTableView?.dataSource = self
            sharesTableView?.delegate = self
            sharesTableView?.reloadData()
        }
    }
    private var kit: NextcloudKit?
    private var itemURL: URL?
    private var shares: [NKShare] = [] {
        didSet { sharesTableView?.reloadData() }
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
        itemURL = url
        Task {
            await reload()
        }
    }

    private func reload() async {
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
            account = convertedAccount
            shares = await fetch(
                itemIdentifier: itemIdentifier, itemRelativePath: serverPath as String
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
                    Logger.sharesDataSource.error("Error fetching shares: \(error)")
                    return
                }
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
}
