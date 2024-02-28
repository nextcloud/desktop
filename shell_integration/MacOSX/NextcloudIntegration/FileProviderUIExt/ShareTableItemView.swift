//
//  ShareTableItemView.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 28/2/24.
//

import AppKit

class ShareTableItemView: NSTableCellView {
    @IBOutlet weak var typeImageView: NSImageView!
    @IBOutlet weak var label: NSTextField!
    @IBOutlet weak var copyLinkButton: NSButton!
    @IBOutlet weak var optionsButton: NSButton!

    override func prepareForReuse() {
        typeImageView.image = nil
        label.stringValue = ""
        copyLinkButton.isHidden = false
        super.prepareForReuse()
    }
}
