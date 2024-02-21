//
//  ShareViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 21/2/24.
//

import AppKit

class ShareViewController: NSViewController {
    @IBOutlet weak var fileNameIcon: NSImageView!
    @IBOutlet weak var fileNameLabel: NSTextField!
    @IBOutlet weak var descriptionLabel: NSTextField!
    @IBOutlet weak var closeButton: NSButton!

    public override var nibName: NSNib.Name? {
        return NSNib.Name(self.className)
    }

    var actionViewController: DocumentActionViewController! {
        return parent as? DocumentActionViewController
    }
}
