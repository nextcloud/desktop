//
//  ShareOptionsView.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 28/2/24.
//

import AppKit
import NextcloudKit

class ShareOptionsView: NSView {
    @IBOutlet private weak var labelTextField: NSTextField!
    @IBOutlet private weak var uploadEditPermissionCheckbox: NSButton!
    @IBOutlet private weak var hideDownloadCheckbox: NSButton!
    @IBOutlet private weak var passwordProtectCheckbox: NSButton!
    @IBOutlet private weak var passwordSecureField: NSSecureTextField!
    @IBOutlet private weak var expirationDateCheckbox: NSButton!
    @IBOutlet private weak var expirationDatePicker: NSDatePicker!
    @IBOutlet private weak var noteForRecipientCheckbox: NSButton!
    @IBOutlet private weak var noteTextField: NSTextField!
    @IBOutlet private weak var saveButton: NSButton!
    @IBOutlet private weak var deleteButton: NSButton!

    var share: NKShare? {
        didSet {
            guard let share = share else { return }
            labelTextField.stringValue = share.label
            uploadEditPermissionCheckbox.state = share.canEdit ? .on : .off
            hideDownloadCheckbox.state = share.hideDownload ? .on : .off
            passwordProtectCheckbox.state = share.password.isEmpty ? .off : .on
            passwordSecureField.isHidden = passwordProtectCheckbox.state == .off
            expirationDateCheckbox.state = share.expirationDate == nil ? .off : .on
            expirationDatePicker.isHidden = expirationDateCheckbox.state == .off
            noteForRecipientCheckbox.state = share.note.isEmpty ? .off : .on
            noteTextField.isHidden = noteForRecipientCheckbox.state == .off
            deleteButton.isEnabled = share.canDelete
        }
    }
}
