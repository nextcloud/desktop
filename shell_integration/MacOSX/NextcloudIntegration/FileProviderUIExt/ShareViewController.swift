//
//  ShareViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 21/2/24.
//

import AppKit
import FileProvider
import NextcloudKit
import OSLog
import QuickLookThumbnailing

class ShareViewController: NSViewController, ShareViewDataSourceUIDelegate {
    let shareDataSource = ShareTableViewDataSource()
    let itemIdentifiers: [NSFileProviderItemIdentifier]

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

    public override var nibName: NSNib.Name? {
        return NSNib.Name(self.className)
    }

    var actionViewController: DocumentActionViewController! {
        return parent as? DocumentActionViewController
    }

    init(_ itemIdentifiers: [NSFileProviderItemIdentifier]) {
        self.itemIdentifiers = itemIdentifiers
        super.init(nibName: nil, bundle: nil)

        guard let firstItem = itemIdentifiers.first else {
            Logger.shareViewController.error("called without items")
            closeAction(self)
            return
        }

        Task {
            await processItemIdentifier(firstItem)
        }
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        dismissError(self)
        hideOptions(self)
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
            await updateDisplay(itemUrl: itemUrl)
            shareDataSource.uiDelegate = self
            shareDataSource.sharesTableView = tableView
            shareDataSource.loadItem(url: itemUrl)
            optionsView.dataSource = shareDataSource
        } catch let error {
            let errorString = "Error processing item: \(error)"
            Logger.shareViewController.error("\(errorString)")
            fileNameLabel.stringValue = "Unknown item"
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
                    Logger.shareViewController.error("Could not get thumbnail: \(error)")
                }
                continuation.resume(returning: thumbnail)
            }
        }
        fileNameIcon.image = fileThumbnail?.nsImage
    }

    @IBAction func dismissError(_ sender: Any) {
        errorMessageStackView.isHidden = true
    }

    @IBAction func createShare(_ sender: Any) {
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
        guard let kit = shareDataSource.kit else { return }
        optionsView.controller = ShareController(share: share, kit: kit)
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
