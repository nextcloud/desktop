//
//  ShareViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 21/2/24.
//

import AppKit
import FileProvider

class ShareViewController: NSViewController {
    let itemIdentifiers: [NSFileProviderItemIdentifier]

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

    init(_ itemIdentifiers: [NSFileProviderItemIdentifier]) {
        self.itemIdentifiers = itemIdentifiers
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
