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

    init(_ itemIdentifiers: [NSFileProviderItemIdentifier], locking: Bool, log: any FileProviderLogging) {
        self.itemIdentifiers = itemIdentifiers
        self.locking = locking
        self.log = log
        self.logger = FileProviderLogger(category: "LockViewController", log: log)
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

        closeButton.title = String(localized: "Close")
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
        logger.error("Error: \(error)")
        descriptionLabel.stringValue = "Error: \(error)"
        stopIndicatingLoading()
    }

    private func fetchCapabilities(account: Account, kit: NextcloudKit) async -> Capabilities? {
        return await withCheckedContinuation { continuation in
            kit.getCapabilities(account: account.ncKitAccount) { account, _, data, error in
                guard error == .success, let capabilitiesJson = data?.data else {
                    self.presentError("Error getting server caps: \(error.errorDescription)")
                    continuation.resume(returning: nil)
                    return
                }

                self.logger.info("Successfully retrieved server share capabilities")
                continuation.resume(returning: Capabilities(data: capabilitiesJson))
            }
        }
    }

    private func processItemIdentifier(_ itemIdentifier: NSFileProviderItemIdentifier) async {
        guard let manager = NSFileProviderManager(for: actionViewController.domain) else {
            fatalError("NSFileProviderManager isn't expected to fail")
        }

        do {
            let itemUrl = try await manager.getUserVisibleURL(for: itemIdentifier)
            guard itemUrl.startAccessingSecurityScopedResource() else {
                logger.error("Could not access scoped resource for item url!")
                return
            }
            await updateFileDetailsDisplay(itemUrl: itemUrl)
            itemUrl.stopAccessingSecurityScopedResource()
            await lockOrUnlockFile(localItemUrl: itemUrl)
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

        guard let itemIdentifier = await withCheckedContinuation({
            (continuation: CheckedContinuation<NSFileProviderItemIdentifier?, Never>) -> Void in
            NSFileProviderManager.getIdentifierForUserVisibleFile(
                at: localItemUrl
            ) { identifier, domainIdentifier, error in
                defer { continuation.resume(returning: identifier) }
                guard error == nil else {
                    self.presentError("No item with identifier: \(error.debugDescription)")
                    return
                }
            }
        }) else {
            presentError("Could not get identifier for item, no shares can be acquired.")
            return
        }

        do {
            let connection = try await serviceConnection(url: localItemUrl, interruptionHandler: {
                self.logger.error("Service connection interrupted")
            })
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
            guard let capabilities = await fetchCapabilities(account: account, kit: kit),
                  capabilities.files?.locking != nil
            else {
                presentError("Server does not have the ability to lock files.")
                return
            }
            guard let itemMetadata = await fetchItemMetadata(
                itemRelativePath: serverPathString, account: account, kit: kit
            ) else {
                presentError("Could not get item metadata.")
                return
            }

            // Run lock state checks
            if locking {
                guard !itemMetadata.lock else {
                    presentError("File is already locked.")
                    return
                }
            } else {
                guard itemMetadata.lock else {
                    presentError("File is already unlocked.")
                    return
                }
            }

            descriptionLabel.stringValue = locking ? String(localized: "Communicating with server, locking file…") : String(localized: "Communicating with server, unlocking file…")

            let serverUrlFileName = itemMetadata.serverUrl + "/" + itemMetadata.fileName
            
            logger.info("Locking file: \(serverUrlFileName) \(self.locking ? "locking" : "unlocking")")

            let error = await withCheckedContinuation { continuation in
                kit.lockUnlockFile(
                    serverUrlFileName: serverUrlFileName,
                    shouldLock: locking,
                    account: account.ncKitAccount,
                    completion: { _, _, error in
                        continuation.resume(returning: error)
                    }
                )
            }

            if error == .success {
                descriptionLabel.stringValue = String(format: self.locking ? String(localized: "File \"%@\" locked!") : String(localized: "File \"%@\" unlocked!"), itemMetadata.fileName)
                warnImage.image = NSImage(systemSymbolName: "checkmark.circle.fill", accessibilityDescription: String(localized: "Checkmark in a circle"))
                stopIndicatingLoading()

                if let manager = NSFileProviderManager(for: actionViewController.domain) {
                    do {
                        try await manager.signalEnumerator(for: itemIdentifier)
                    } catch let error {
                        presentError(
                            """
                            Could not signal lock state change in virtual file.
                            Changes may take a while to be reflected on your Mac.
                            Error: \(error.localizedDescription)
                            """)
                    }
                }
            } else {
                presentError("Could not lock file: \(error.errorDescription).")
            }
        } catch let error {
            presentError("Could not lock file: \(error).")
        }
    }
}
