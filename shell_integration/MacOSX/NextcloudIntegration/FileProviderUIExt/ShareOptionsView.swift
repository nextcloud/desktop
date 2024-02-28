//
//  ShareOptionsView.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 28/2/24.
//

import AppKit

class ShareOptionsView: NSView {
    @IBOutlet weak var labelTextField: NSTextField!
    @IBOutlet weak var uploadEditPermissionCheckbox: NSButton!
    @IBOutlet weak var hideDownloadCheckbox: NSButton!
    @IBOutlet weak var passwordProtectCheckbox: NSButton!
    @IBOutlet weak var passwordSecureField: NSSecureTextField!
    @IBOutlet weak var expirationDateCheckbox: NSButton!
    @IBOutlet weak var expirationDatePicker: NSDatePicker!
    @IBOutlet weak var noteForRecipientCheckbox: NSButton!
    @IBOutlet weak var noteTextField: NSTextField!
    @IBOutlet weak var saveButton: NSButton!
    @IBOutlet weak var deleteButton: NSButton!
}
