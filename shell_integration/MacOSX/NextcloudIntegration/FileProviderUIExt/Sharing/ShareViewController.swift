//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import AppKit
import FileProvider
import NextcloudFileProviderKit
import NextcloudKit
import OSLog
import QuickLookThumbnailing

class ShareViewController: NSViewController, ShareViewDataSourceUIDelegate {
    let shareDataSource: ShareTableViewDataSource
    let itemIdentifiers: [NSFileProviderItemIdentifier]
    let log: any FileProviderLogging
    let logger: FileProviderLogger

    @IBOutlet weak var fileNameIcon: NSImageView!
    @IBOutlet weak var fileNameLabel: NSTextField!
    @IBOutlet weak var descriptionLabel: NSTextField!
    @IBOutlet weak var createButton: NSButton!
    @IBOutlet weak var closeButton: NSButton!
    @IBOutlet weak var tableView: NSTableView!
    @IBOutlet weak var optionsView: ShareOptionsView!
    @IBOutlet weak var splitView: NSSplitView!
    @IBOutlet weak var loadingEffectView: NSVisualEffectView!
    @IBOutlet weak var loadingIndicator: NSProgressIndicator!
    @IBOutlet weak var errorMessageStackView: NSStackView!
    @IBOutlet weak var errorTextLabel: NSTextField!
    @IBOutlet weak var errorDismissButton: NSButton!
    @IBOutlet weak var noSharesLabel: NSTextField!

    public override var nibName: NSNib.Name? {
        return NSNib.Name(self.className)
    }

    var actionViewController: DocumentActionViewController! {
        return parent as? DocumentActionViewController
    }

    init(_ itemIdentifiers: [NSFileProviderItemIdentifier], log: any FileProviderLogging) {
        self.itemIdentifiers = itemIdentifiers
        self.log = log
        self.logger = FileProviderLogger(category: "ShareViewController", log: log)
        self.shareDataSource = ShareTableViewDataSource(log: log)
        super.init(nibName: nil, bundle: nil)

        guard let firstItem = itemIdentifiers.first else {
            logger.error("called without items")
            closeAction(self)
            return
        }

        logger.info("Instantiated with itemIdentifiers:  \(itemIdentifiers.map { $0.rawValue })")

        Task {
            await processItemIdentifier(firstItem)
        }
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        errorDismissButton.title = String(localized: "Dismiss")
        dismissError(self)
        hideOptions(self)
        optionsView.applyLocalizedStrings()
    }

    @IBAction func closeAction(_ sender: Any) {
        actionViewController.extensionContext.completeRequest()
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

            await updateDisplay(itemUrl: itemUrl)
            shareDataSource.uiDelegate = self
            shareDataSource.sharesTableView = tableView
            shareDataSource.loadItem(url: itemUrl)
            optionsView.dataSource = shareDataSource
            itemUrl.stopAccessingSecurityScopedResource()
        } catch let error {
            let errorString = "Error processing item: \(error)"
            logger.error("\(errorString)")
            fileNameLabel.stringValue = String(localized: "Unknown item")
            descriptionLabel.stringValue = errorString
        }
    }

    private func updateDisplay(itemUrl: URL) async {
        fileNameLabel.stringValue = itemUrl.lastPathComponent

        let request = QLThumbnailGenerator.Request(
            fileAt: itemUrl,
            size: CGSize(width: 128, height: 128),
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
        fileNameIcon.image = fileThumbnail?.nsImage

        let resourceValues = try? itemUrl.resourceValues(
            forKeys: [.fileSizeKey, .contentModificationDateKey]
        )

        var sizeDesc = String(localized: "Unknown size")
        var modDesc = String(localized: "Unknown modification date")

        if let fileSize = resourceValues?.fileSize {
            sizeDesc = ByteCountFormatter().string(fromByteCount: Int64(fileSize))
        }

        if let modificationDate = resourceValues?.contentModificationDate {
            let modDateString = DateFormatter.localizedString(from: modificationDate, dateStyle: .short, timeStyle: .short)
            modDesc = String(format: String(localized: "Last modified: %@"), modDateString)
        }
        
        descriptionLabel.stringValue = "\(sizeDesc) · \(modDesc)"
    }

    @IBAction func dismissError(_ sender: Any) {
        errorMessageStackView.isHidden = true
    }

    @IBAction func createShare(_ sender: Any) {
        guard let account = shareDataSource.account else { return }
        optionsView.account = account
        optionsView.createMode = true
        tableView.deselectAll(self)
        if !splitView.arrangedSubviews.contains(optionsView) {
            splitView.addArrangedSubview(optionsView)
            optionsView.isHidden = false
        }
    }

    func fetchStarted() {
        loadingEffectView.isHidden = false
        loadingIndicator.startAnimation(self)
    }

    func fetchFinished() {
        noSharesLabel.isHidden = !shareDataSource.shares.isEmpty
        loadingEffectView.isHidden = true
        loadingIndicator.stopAnimation(self)
    }

    func hideOptions(_ sender: Any) {
        if sender as? ShareTableViewDataSource == shareDataSource, optionsView.createMode {
            // Do not hide options if the table view has had everything deselected when we set the
            // options view to be in create mode
            return
        }
        splitView.removeArrangedSubview(optionsView)
        optionsView.isHidden = true
    }

    func showOptions(share: NKShare) {
        guard let account = shareDataSource.account, share.canEdit || share.canDelete else { return }
        optionsView.account = account
        optionsView.controller = ShareController(share: share, account: account, kit: shareDataSource.kit, log: log)

        if !splitView.arrangedSubviews.contains(optionsView) {
            splitView.addArrangedSubview(optionsView)
            optionsView.isHidden = false
        }
    }

    func showError(_ errorString: String) {
        errorMessageStackView.isHidden = false
        errorTextLabel.stringValue = errorString
    }
}
