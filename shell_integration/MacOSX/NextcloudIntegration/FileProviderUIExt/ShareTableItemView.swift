//
//  ShareTableItemView.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 28/2/24.
//

import AppKit
import NextcloudKit

class ShareTableItemView: NSTableCellView {
    @IBOutlet private weak var typeImageView: NSImageView!
    @IBOutlet private weak var label: NSTextField!
    @IBOutlet private weak var copyLinkButton: NSButton!
    @IBOutlet private weak var optionsButton: NSButton!

    var share: NKShare? {
        didSet {
            guard let share = share else {
                prepareForReuse()
                return
            }
            typeImageView.image = share.typeImage
            label.stringValue = share.displayString
            copyLinkButton.isHidden = share.shareType != NKShare.ShareType.publicLink.rawValue
        }
    }

    override func prepareForReuse() {
        typeImageView.image = nil
        label.stringValue = ""
        copyLinkButton.isHidden = false
        super.prepareForReuse()
    }
}
