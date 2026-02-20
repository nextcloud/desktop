//
//  ShareTableItemView.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import AppKit
import NextcloudKit

class ShareTableItemView: NSTableCellView {
    @IBOutlet private weak var typeImageView: NSImageView!
    @IBOutlet private weak var label: NSTextField!
    @IBOutlet private weak var copyLinkButton: NSButton!
    private var originalCopyImage: NSImage?
    private var copiedButtonImage: NSImage?
    private var tempButtonTimer: Timer?

    var share: NKShare? {
        didSet {
            guard let share = share else {
                prepareForReuse()
                return
            }
            typeImageView.image = share.typeImage
            label.stringValue = share.displayString
            copyLinkButton.isHidden =
                share.shareType != NKShare.ShareType.publicLink.rawValue &&
                share.shareType != NKShare.ShareType.internalLink.rawValue
        }
    }

    override func prepareForReuse() {
        typeImageView.image = nil
        label.stringValue = ""
        copyLinkButton.isHidden = false
        super.prepareForReuse()
    }

    @IBAction func copyShareLink(sender: Any) {
        guard let share = share else { return }
        let pasteboard = NSPasteboard.general
        pasteboard.declareTypes([.string], owner: nil)
        pasteboard.setString(share.url, forType: .string)

        guard tempButtonTimer == nil else { return }

        originalCopyImage = copyLinkButton.image
        copiedButtonImage = NSImage(
            systemSymbolName: "checkmark.circle.fill",
            accessibilityDescription: String(localized: "Public link has been copied icon")
        )
        var config = NSImage.SymbolConfiguration(scale: .medium)
        config = config.applying(.init(hierarchicalColor: .systemGreen))
        copiedButtonImage = copiedButtonImage?.withSymbolConfiguration(config)
        copyLinkButton.image = copiedButtonImage
        tempButtonTimer = Timer.scheduledTimer(withTimeInterval: 3.0, repeats: false) { timer in
            self.copyLinkButton.image = self.originalCopyImage
            self.copiedButtonImage = nil
            self.tempButtonTimer?.invalidate()
            self.tempButtonTimer = nil
        }
    }
}
