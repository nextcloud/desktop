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

    var controller: ShareController? {
        didSet {
            update()

        }
    }

    private func update() {
        guard let share = controller?.share else {
            reset()
            saveButton.isEnabled = false
            deleteButton.isEnabled = false
            return
        }
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
        saveButton.isEnabled = true
    }

    func reset() {
        labelTextField.stringValue = ""
        uploadEditPermissionCheckbox.state = .off
        hideDownloadCheckbox.state = .off
        passwordProtectCheckbox.state = .off
        passwordSecureField.isHidden = true
        expirationDateCheckbox.state = .off
        expirationDatePicker.isHidden = true
        noteForRecipientCheckbox.state = .off
        noteTextField.isHidden = true
    }
}
