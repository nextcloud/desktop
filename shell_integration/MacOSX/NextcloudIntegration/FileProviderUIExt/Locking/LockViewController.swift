//
//  LockViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 30/7/24.
//

import AppKit
import FileProvider
import NextcloudFileProviderKit
import NextcloudKit
import OSLog
import QuickLookThumbnailing

class LockViewController: NSViewController {
    let itemIdentifiers: [NSFileProviderItemIdentifier]
    let locking: Bool

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

    init(_ itemIdentifiers: [NSFileProviderItemIdentifier], locking: Bool) {
        self.itemIdentifiers = itemIdentifiers
        self.locking = locking
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        guard let firstItem = itemIdentifiers.first else {
            Logger.shareViewController.error("called without items")
            closeAction(self)
            return
        }

        Logger.lockViewController.info(
            """
            Locking \(self.locking ? "enabled" : "disabled", privacy: .public) for items:
            \(firstItem.rawValue, privacy: .public)
            """
        )

        Task {
            await processItemIdentifier(firstItem)
        }
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
        Logger.lockViewController.error("Error: \(error, privacy: .public)")
        descriptionLabel.stringValue = "Error: \(error)"
        stopIndicatingLoading()
    }

    private func processItemIdentifier(_ itemIdentifier: NSFileProviderItemIdentifier) async {
        guard let manager = NSFileProviderManager(for: actionViewController.domain) else {
            fatalError("NSFileProviderManager isn't expected to fail")
        }

        do {
            let itemUrl = try await manager.getUserVisibleURL(for: itemIdentifier)
            guard itemUrl.startAccessingSecurityScopedResource() else {
                Logger.lockViewController.error("Could not access scoped resource for item url!")
                return
            }
            await updateFileDetailsDisplay(itemUrl: itemUrl)
            itemUrl.stopAccessingSecurityScopedResource()
            await lockOrUnlockFile(localItemUrl: itemUrl)
        } catch let error {
            let errorString = "Error processing item: \(error)"
            Logger.lockViewController.error("\(errorString, privacy: .public)")
            fileNameLabel.stringValue = "Could not lock unknown item…"
            descriptionLabel.stringValue = errorString
        }
    }

    private func updateFileDetailsDisplay(itemUrl: URL) async {
        let lockAction = locking ? "Locking" : "Unlocking"
        fileNameLabel.stringValue = "\(lockAction) file \(itemUrl.lastPathComponent)…"

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
                    Logger.lockViewController.error(
                        "Could not get thumbnail: \(error, privacy: .public)"
                    )
                }
                continuation.resume(returning: thumbnail)
            }
        }

        fileNameIcon.image =
            fileThumbnail?.nsImage ?? 
            NSImage(systemSymbolName: "doc", accessibilityDescription: "doc")
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
                Logger.lockViewController.error("Service connection interrupted")
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
                nextcloudVersion: 25,
                groupIdentifier: ""
            )
            // guard let capabilities = await fetchCapabilities() else {
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

            descriptionLabel.stringValue =
                "Communicating with server, \(locking ? "locking" : "unlocking") file…"

            let serverUrlFileName = itemMetadata.serverUrl + "/" + itemMetadata.fileName
            Logger.lockViewController.info(
                """
                Locking file: \(serverUrlFileName, privacy: .public)
                \(self.locking ? "locking" : "unlocking", privacy: .public)
                """
            )

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
                descriptionLabel.stringValue = "File \(self.locking ? "locked" : "unlocked")!"
                warnImage.image = NSImage(
                    systemSymbolName: "checkmark.circle.fill",
                    accessibilityDescription: "checkmark.circle.fill"
                )
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
