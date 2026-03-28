//
//  LockViewController.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import AppKit
import FileProvider
import NextcloudCapabilitiesKit
import NextcloudFileProviderKit
import NextcloudKit
import OSLog
import QuickLookThumbnailing

class LockViewController: NSViewController {
    let itemIdentifiers: [NSFileProviderItemIdentifier]
    let locking: Bool
    let log: any FileProviderLogging
    let logger: FileProviderLogger
    let serviceResolver: ServiceResolver

    @IBOutlet weak var fileNameIcon: NSImageView!
    @IBOutlet weak var fileNameLabel: NSTextField!
    @IBOutlet weak var descriptionLabel: NSTextField!
    @IBOutlet weak var closeButton: NSButton!
    @IBOutlet weak var loadingIndicator: NSProgressIndicator!
    @IBOutlet weak var warnImage: NSImageView!

    public override var nibName: NSNib.Name? {
        return NSNib.Name(self.className)
    }

    var actionViewController: DocumentActionViewController! {
        return parent as? DocumentActionViewController
    }

    init(_ itemIdentifiers: [NSFileProviderItemIdentifier], locking: Bool, serviceResolver: ServiceResolver, log: any FileProviderLogging) {
        self.itemIdentifiers = itemIdentifiers
        self.locking = locking
        self.log = log
        self.logger = FileProviderLogger(category: "LockViewController", log: log)
        self.serviceResolver = serviceResolver

        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        guard let firstItem = itemIdentifiers.first else {
            logger.error("called without items")
            closeAction(self)
            return
        }

        logger.info("Locking \(self.locking ? "enabled" : "disabled") for items: \(firstItem.rawValue)")

        Task {
            await processItemIdentifier(firstItem)
        }

        closeButton.setAccessibilityTitle(String(localized: "Close"))
    }

    @IBAction func closeAction(_ sender: Any) {
        actionViewController.extensionContext.completeRequest()
    }

    private func stopIndicatingLoading() {
        loadingIndicator.stopAnimation(self)
        loadingIndicator.isHidden = true
        warnImage.isHidden = false
    }

    private func presentError(_ error: String) {
        logger.error("Presenting error: \(error)")
        descriptionLabel.stringValue = error
        warnImage.contentTintColor = .systemRed
        stopIndicatingLoading()
    }

    private func fetchCapabilities(account: Account, kit: NextcloudKit) async -> Capabilities? {
        return await withCheckedContinuation { continuation in
            kit.getCapabilities(account: account.ncKitAccount) { account, _, data, error in
                guard error == .success, let capabilitiesJson = data?.data else {
                    self.presentError("Failed to fetch server capabilities: \(error.errorDescription)")
                    continuation.resume(returning: nil)
                    return
                }

                self.logger.info("Successfully retrieved server share capabilities.")
                continuation.resume(returning: Capabilities(data: capabilitiesJson))
            }
        }
    }

    private func processItemIdentifier(_ itemIdentifier: NSFileProviderItemIdentifier) async {
        guard let manager = NSFileProviderManager(for: actionViewController.domain) else {
            fatalError("Failed to initialize file provider manager for domain with identifier \"\(actionViewController.domain.identifier)\"!")
        }

        do {
            let itemUrl = try await manager.getUserVisibleURL(for: itemIdentifier)

            guard itemUrl.startAccessingSecurityScopedResource() else {
                logger.error("Could not access scoped resource for item url!")
                return
            }

            await updateFileDetailsDisplay(itemUrl: itemUrl)
            await lockOrUnlockFile(localItemUrl: itemUrl)
            itemUrl.stopAccessingSecurityScopedResource()
        } catch let error {
            let errorString = "Error processing item: \(error)"
            logger.error("\(errorString)")
            fileNameLabel.stringValue = String(localized: "Could not lock unknown item…")
            descriptionLabel.stringValue = error.localizedDescription
        }
    }

    private func updateFileDetailsDisplay(itemUrl: URL) async {
        fileNameLabel.stringValue = locking ? String(format: String(localized: "Locking file \"%@\"…"), itemUrl.lastPathComponent) : String(format: String(localized: "Unlocking file \"%@\"…"), itemUrl.lastPathComponent)

        let request = QLThumbnailGenerator.Request(
            fileAt: itemUrl,
            size: CGSize(width: 48, height: 48),
            scale: 1.0,
            representationTypes: .icon
        )

        let generator = QLThumbnailGenerator.shared

        let fileThumbnail = await withCheckedContinuation { continuation in
            generator.generateRepresentations(for: request) { thumbnail, type, error in
                if thumbnail == nil || error != nil {
                    self.logger.error("Could not get thumbnail.", [.error: error])
                }
                continuation.resume(returning: thumbnail)
            }
        }

        fileNameIcon.image = fileThumbnail?.nsImage ?? NSImage(systemSymbolName: "doc", accessibilityDescription: String(localized: "Document symbol"))
    }

    private func lockOrUnlockFile(localItemUrl: URL) async {
        descriptionLabel.stringValue = "Fetching file details…"

        var itemIdentifier: NSFileProviderItemIdentifier?

        do {
            (itemIdentifier, _) = try await NSFileProviderManager.identifierForUserVisibleFile(at: localItemUrl)
        } catch {
            self.presentError("No item with identifier: \(error.localizedDescription)")
            return
        }

        guard let itemIdentifier else {
            self.presentError("Failed to get identifier for file at \(localItemUrl.path)!")
            return
        }

        do {
            let connection = try await serviceResolver.getService(at: localItemUrl)

            guard let serverPath = await connection.itemServerPath(identifier: itemIdentifier),
                  let credentials = await connection.credentials() as? Dictionary<String, String>,
                  let account = Account(dictionary: credentials),
                  !account.password.isEmpty
            else {
                presentError("Failed to get details from File Provider Extension.")
                return
            }

            let serverPathString = serverPath as String
            let kit = NextcloudKit.shared

            kit.appendSession(
                account: account.ncKitAccount,
                urlBase: account.serverUrl,
                user: account.username,
                userId: account.id,
                password: account.password,
                userAgent: "Nextcloud-macOS/FileProviderUIExt",
                groupIdentifier: ""
            )

            guard let capabilities = await fetchCapabilities(account: account, kit: kit), capabilities.files?.locking != nil else {
                presentError("Server does not have the ability to lock files.")
                return
            }

            guard let itemMetadata = await fetchItemMetadata(itemRelativePath: serverPathString, account: account, kit: kit) else {
                presentError("Could not get item metadata.")
                return
            }

            // Run lock state checks
            if locking {
                guard itemMetadata.lock == false else {
                    presentError("File is already locked.")
                    return
                }
            } else {
                guard itemMetadata.lock == true else {
                    presentError("File is already unlocked.")
                    return
                }
            }

            descriptionLabel.stringValue = locking ? String(localized: "Communicating with server, locking file…") : String(localized: "Communicating with server, unlocking file…")

            let serverUrlFileName = itemMetadata.serverUrl + "/" + itemMetadata.fileName

            logger.info("About to \(self.locking ? "lock" : "unlock")...", [.item: itemIdentifier, .name: itemMetadata.fileName, .url: serverUrlFileName])

            do {
                let _ = try await kit.lockUnlockFile(serverUrlFileName: serverUrlFileName, shouldLock: locking, account: account.ncKitAccount)
                logger.info(locking ? "Successfully locked file." : "Successfully unlocked file.", [.item: itemIdentifier, .name: itemMetadata.fileName, .url: serverUrlFileName])
            } catch {
                presentError("Could not lock file: \(error.localizedDescription)")
            }

            descriptionLabel.stringValue = String(format: self.locking ? String(localized: "File \"%@\" locked!") : String(localized: "File \"%@\" unlocked!"), itemMetadata.fileName)
            warnImage.image = NSImage(systemSymbolName: "checkmark.circle.fill", accessibilityDescription: String(localized: "Checkmark in a circle"))
            warnImage.contentTintColor = .systemGreen
            stopIndicatingLoading()

            if let manager = NSFileProviderManager(for: actionViewController.domain) {
                do {
                    try await manager.signalEnumerator(for: .workingSet)
                    logger.info("Signalled enumerator for item.", [.item: NSFileProviderItemIdentifier.workingSet])
                } catch let error {
                    logger.error("Signaling enumerator for item failed.", [.error: error, .item: NSFileProviderItemIdentifier.workingSet])
                    presentError("Could not signal lock state change in file provider item. Changes may take a while to be reflected on your Mac.")
                }
            }
        } catch let error {
            presentError("Could not lock file: \(error).")
        }
    }
}
