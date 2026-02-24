//
//  ShareTableViewDataSource.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import AppKit
import FileProvider
import NextcloudKit
import NextcloudFileProviderKit
import NextcloudCapabilitiesKit
import OSLog

class ShareTableViewDataSource: NSObject, NSTableViewDataSource, NSTableViewDelegate {
    private let shareItemViewIdentifier = NSUserInterfaceItemIdentifier("ShareTableItemView")
    private let shareItemViewNib = NSNib(nibNamed: "ShareTableItemView", bundle: nil)
    private let reattemptInterval: TimeInterval = 3.0

    let kit = NextcloudKit.shared
    let logger: FileProviderLogger
    let serviceResolver: ServiceResolver

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

    var capabilities: Capabilities?

    private(set) var itemURL: URL?
    private(set) var itemServerRelativePath: String?

    private(set) var shares: [NKShare] = [] {
        didSet {
            Task { @MainActor in
                sharesTableView?.reloadData()
            }
        }
    }

    private(set) var userAgent: String = "Nextcloud-macOS/FileProviderUIExt"

    private(set) var account: Account? {
        didSet {
            guard let account = account else {
                return
            }

            kit.appendSession(
                account: account.ncKitAccount,
                urlBase: account.serverUrl,
                user: account.username,
                userId: account.username,
                password: account.password,
                userAgent: userAgent,
                groupIdentifier: ""
            )
        }
    }

    init(serviceResolver: ServiceResolver, log: any FileProviderLogging) {
        self.logger = FileProviderLogger(category: "ShareTableViewDataSource", log: log)
        self.serviceResolver = serviceResolver
    }

    func loadItem(url: URL) {
        itemServerRelativePath = nil
        itemURL = url

        Task {
            await reload()
        }
    }

    func reattempt() {
        DispatchQueue.main.async {
            Timer.scheduledTimer(withTimeInterval: self.reattemptInterval, repeats: false) { _ in
                Task { await self.reload() }
            }
        }
    }

    func reload() async {
        guard let itemURL else {
            presentError(String(localized: "No item URL, cannot reload data!"))
            return
        }

        guard let (itemIdentifier, _) = try? await NSFileProviderManager.identifierForUserVisibleFile(at: itemURL) else {
            presentError(String(localized: "Could not get identifier for item, no shares can be acquired."))
            return
        }

        do {
            let connection = try await serviceResolver.getService(at: itemURL)

            if let acquiredUserAgent = await connection.userAgent() {
                userAgent = acquiredUserAgent as String
            }

            guard let serverPath = await connection.itemServerPath(identifier: itemIdentifier),
                  let credentials = await connection.credentials() as? Dictionary<String, String>,
                  let convertedAccount = Account(dictionary: credentials),
                  !convertedAccount.password.isEmpty
            else {
                presentError(String(localized: "Failed to get details from File Provider Extension. Retrying."))
                reattempt()
                return
            }

            let serverPathString = serverPath as String
            itemServerRelativePath = serverPathString
            account = convertedAccount
            await sharesTableView?.deselectAll(self)
            capabilities = await fetchCapabilities()

            guard capabilities != nil else {
                return
            }

            guard capabilities?.filesSharing?.apiEnabled == true else {
                presentError(String(localized: "Server does not support shares."))
                return
            }

            guard let account else {
                presentError(String(localized: "Account data is unavailable, cannot reload data!"))
                return
            }

            guard let itemMetadata = await fetchItemMetadata(
                itemRelativePath: serverPathString, account: account, kit: kit
            ) else {
                presentError(String(localized: "Unable to retrieve file metadata…"))
                return
            }

            guard itemMetadata.permissions.contains("R") == true else {
                presentError(String(localized: "This file cannot be shared."))
                return
            }

            shares = await fetch(itemIdentifier: itemIdentifier, itemRelativePath: serverPathString)
            shares.append(Self.generateInternalShare(for: itemMetadata))
        } catch let error {
            presentError(String(format: String(localized: "Could not reload data: %@, will try again."), error.localizedDescription))
            reattempt()
        }
    }

    private func fetch(itemIdentifier: NSFileProviderItemIdentifier, itemRelativePath: String) async -> [NKShare] {
        Task { @MainActor in
            uiDelegate?.fetchStarted()
        }

        defer {
            Task { @MainActor in
                uiDelegate?.fetchFinished()
            }
        }

        let rawIdentifier = itemIdentifier.rawValue
        logger.info("Fetching shares for item \(rawIdentifier)")

        guard let account else {
            self.presentError(String(localized: "NextcloudKit instance or account is unavailable, cannot fetch shares!"))
            return []
        }

        let parameter = NKShareParameter(path: itemRelativePath)

        return await withCheckedContinuation { continuation in
            kit.readShares(parameters: parameter, account: account.ncKitAccount) { account, shares, data, error in
                let shareCount = shares?.count ?? 0
                self.logger.info("Received \(shareCount) shares")

                defer {
                    continuation.resume(returning: shares ?? [])
                }

                guard error == .success else {
                    self.presentError(String(localized: "Error fetching shares: \(error.errorDescription)"))
                    return
                }
            }
        }
    }

    private static func generateInternalShare(for file: NKFile) -> NKShare {
        let internalShare = NKShare()
        internalShare.shareType = NKShare.ShareType.internalLink.rawValue
        internalShare.url = file.urlBase +  "/index.php/f/" + file.fileId
        internalShare.account = file.account
        internalShare.displaynameOwner = file.ownerDisplayName
        internalShare.displaynameFileOwner = file.ownerDisplayName
        internalShare.path = file.path

        return internalShare
    }

    private func fetchCapabilities() async -> Capabilities? {
        guard let account else {
            self.presentError(String(localized: "Could not fetch capabilities as account is invalid."))
            return nil
        }

        return await withCheckedContinuation { continuation in
            kit.getCapabilities(account: account.ncKitAccount) { account, _, data, error in
                guard error == .success, let capabilitiesJson = data?.data else {
                    self.presentError(String(localized: "Error getting server caps: \(error.errorDescription)"))
                    continuation.resume(returning: nil)
                    return
                }

                self.logger.info("Successfully retrieved server share capabilities")
                continuation.resume(returning: Capabilities(data: capabilitiesJson))
            }
        }
    }

    private func presentError(_ errorString: String) {
        logger.error("\(errorString)")

        Task { @MainActor in
            self.uiDelegate?.showError(errorString)
        }
    }

    // MARK: - NSTableViewDataSource protocol methods

    @objc func numberOfRows(in tableView: NSTableView) -> Int {
        shares.count
    }

    // MARK: - NSTableViewDelegate protocol methods

    @objc func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        let share = shares[row]

        guard let view = tableView.makeView(withIdentifier: shareItemViewIdentifier, owner: self) as? ShareTableItemView else {
            logger.error("Acquired item view from table is not a share item view!")
            return nil
        }

        view.share = share
        return view
    }

    @objc func tableViewSelectionDidChange(_ notification: Notification) {
        guard let selectedRow = sharesTableView?.selectedRow, selectedRow >= 0 else {
            Task { @MainActor in
                uiDelegate?.hideOptions(self)
            }

            return
        }

        let share = shares[selectedRow]

        Task { @MainActor in
            uiDelegate?.showOptions(share: share)
        }
    }
}
