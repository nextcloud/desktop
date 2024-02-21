//
//  ShareViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 21/2/24.
//

import AppKit

class ShareViewController: NSViewController {
    @IBOutlet weak var identifierLabel: NSTextField!
    @IBOutlet weak var actionTypeLabel: NSTextField!

    public override var nibName: NSNib.Name? {
        return NSNib.Name(self.className)
    }

    var actionViewController: DocumentActionViewController! {
        return parent as? DocumentActionViewController
    }
}
