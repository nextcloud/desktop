//
//  ShareOptionsView.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 28/2/24.
//

import AppKit
import Combine
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
    @IBOutlet private weak var shareTypePicker: NSPopUpButton!
    @IBOutlet private weak var publicLinkShareMenuItem: NSMenuItem!
    @IBOutlet private weak var userShareMenuItem: NSMenuItem!
    @IBOutlet private weak var groupShareMenuItem: NSMenuItem!
    @IBOutlet private weak var emailShareMenuItem: NSMenuItem!
    @IBOutlet private weak var federatedCloudShareMenuItem: NSMenuItem!
    @IBOutlet private weak var circleShare: NSMenuItem!
    @IBOutlet private weak var talkConversationShare: NSMenuItem!

    var dataSource: ShareTableViewDataSource?
    var controller: ShareController? {
        didSet {
            guard controller != nil else { return }
            cancellable?.cancel()
            createMode = false
            update()
            cancellable = controller.publisher.sink { _ in self.update() }
        }
    }
    var createMode = false {
        didSet {
            Logger.shareOptionsView.info("Create mode set: \(self.createMode)")
            shareTypePicker.isHidden = !createMode
            labelTextField.isHidden = createMode  // Cannot set label on create API call
            guard createMode else { return }
            cancellable?.cancel()
            cancellable = nil
            controller = nil
            reset()
            setAllFields(enabled: true)
        }
    }
    private var cancellable: AnyCancellable?

    private func update() {
        guard let share = controller?.share else {
            reset()
            setAllFields(enabled: false)
            saveButton.isEnabled = false
            deleteButton.isEnabled = false
            return
        }

        deleteButton.isEnabled = share.canDelete
        saveButton.isEnabled = share.canEdit

        if share.canEdit {
            setAllFields(enabled: true)
            labelTextField.stringValue = share.label
            uploadEditPermissionCheckbox.state = share.shareesCanEdit ? .on : .off
            hideDownloadCheckbox.state = share.hideDownload ? .on : .off
            passwordProtectCheckbox.state = share.password.isEmpty ? .off : .on
            passwordSecureField.isHidden = passwordProtectCheckbox.state == .off
            expirationDateCheckbox.state = share.expirationDate == nil ? .off : .on
            expirationDatePicker.isHidden = expirationDateCheckbox.state == .off
            noteForRecipientCheckbox.state = share.note.isEmpty ? .off : .on
            noteTextField.isHidden = noteForRecipientCheckbox.state == .off
        } else {
            setAllFields(enabled: false)
            reset()
        }
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

    func setAllFields(enabled: Bool) {
        labelTextField.isEnabled = enabled
        uploadEditPermissionCheckbox.isEnabled = enabled
        hideDownloadCheckbox.isEnabled = enabled
        passwordProtectCheckbox.isEnabled = enabled
        passwordSecureField.isEnabled = enabled
        expirationDateCheckbox.isEnabled = enabled
        expirationDatePicker.isEnabled = enabled
        noteForRecipientCheckbox.isEnabled = enabled
        noteTextField.isEnabled = enabled
        saveButton.isEnabled = enabled
        deleteButton.isEnabled = enabled
    }

    @IBAction func save(_ sender: Any) {
        Task { @MainActor in
            guard let controller = controller else { return }
            let share = controller.share
            let password = passwordProtectCheckbox.state == .on
                ? passwordSecureField.stringValue
                : ""
            let expireDate = expirationDateCheckbox.state == .on
                ? NKShare.formattedDateString(date: expirationDatePicker.dateValue)
                : ""
            let note = noteForRecipientCheckbox.state == .on
                ? noteTextField.stringValue
                : ""
            let label = labelTextField.stringValue
            let hideDownload = hideDownloadCheckbox.state == .on
            let uploadAndEdit = uploadEditPermissionCheckbox.state == .on
            let permissions = uploadAndEdit
                ? share.permissions | NKShare.PermissionValues.updateShare.rawValue
                : share.permissions & ~NKShare.PermissionValues.updateShare.rawValue

            setAllFields(enabled: false)
            deleteButton.isEnabled = false
            saveButton.isEnabled = false
            let error = await controller.save(
                password: password,
                expireDate: expireDate,
                permissions: permissions,
                note: note,
                label: label,
                hideDownload: hideDownload
            )
            if let error = error, error != .success {
                dataSource?.uiDelegate?.showError("Error updating share: \(error.errorDescription)")
            }
            await dataSource?.reload()
        }
    }

    @IBAction func delete(_ sender: Any) {
        Task { @MainActor in
            setAllFields(enabled: false)
            deleteButton.isEnabled = false
            saveButton.isEnabled = false
            let error = await controller?.delete()
            if let error = error, error != .success {
                dataSource?.uiDelegate?.showError("Error deleting share: \(error.errorDescription)")
            }
            await dataSource?.reload()
        }
    }
}
