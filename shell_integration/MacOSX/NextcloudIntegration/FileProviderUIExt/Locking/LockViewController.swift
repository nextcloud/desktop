//
//  LockViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 30/7/24.
//

import AppKit
import FileProvider
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

    @IBAction func closeAction(_ sender: Any) {
        actionViewController.extensionContext.completeRequest()
    }

    private func presentError(_ error: String) {
        Logger.lockViewController.error("Error: \(error, privacy: .public)")
        descriptionLabel.stringValue = "Error: \(error)"
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
}
