//
//  ShareOptionsView.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import AppKit
import Combine
import NextcloudFileProviderKit
import NextcloudKit
import OSLog
import SuggestionsTextFieldKit

class ShareOptionsView: NSView {
    var logger: FileProviderLogger?

    @IBOutlet private weak var optionsTitleTextField: NSTextField!
    @IBOutlet private weak var shareRecipientTextField: NSTextField!  // Hide if public link share
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

    // Share type picker options
    @IBOutlet private weak var publicLinkShareMenuItem: NSMenuItem!
    @IBOutlet private weak var userShareMenuItem: NSMenuItem!
    @IBOutlet private weak var groupShareMenuItem: NSMenuItem!
    @IBOutlet private weak var emailShareMenuItem: NSMenuItem!
    @IBOutlet private weak var federatedCloudShareMenuItem: NSMenuItem!
    @IBOutlet private weak var teamShare: NSMenuItem!
    @IBOutlet private weak var talkConversationShare: NSMenuItem!

    let kit = NextcloudKit.shared
    var account: Account? {
        didSet {
            logger?.info("Setting up account.")

            guard let account else {
                logger?.error("Could not configure suggestions data source.")
                return
            }

            guard let controller else {
                return
            }

            suggestionsTextFieldDelegate.suggestionsDataSource = ShareeSuggestionsDataSource(account: account, kit: kit, log: controller.log)

            suggestionsTextFieldDelegate.confirmationHandler = { suggestion in
                guard let sharee = suggestion?.data as? NKSharee else {
                    return
                }

                self.shareRecipientTextField.stringValue = sharee.shareWith
                self.logger?.debug("Chose sharee \(sharee.shareWith)")
            }

            suggestionsTextFieldDelegate.targetTextField = shareRecipientTextField
        }
    }
    var dataSource: ShareTableViewDataSource?
    var controller: ShareController? {
        didSet {
            guard controller != nil else { return }
            optionsTitleTextField.stringValue = String(localized: "Share options")
            deleteButton.title = String(localized: "Delete")
            deleteButton.image = NSImage(systemSymbolName: "trash", accessibilityDescription: String(localized: "Delete trash icon"))
            deleteButton.bezelColor = NSColor.systemRed
            cancellable?.cancel()
            createMode = false
            update()
            cancellable = controller.publisher.sink { _ in self.update() }
        }
    }
    var createMode = false {
        didSet {
            logger?.info("Create mode set: \(self.createMode)")
            shareTypePicker.isHidden = !createMode
            shareRecipientTextField.isHidden = !createMode
            labelTextField.isHidden = createMode  // Cannot set label on create API call
            guard createMode else { return }
            optionsTitleTextField.stringValue = String(localized: "Create new share")
            deleteButton.title = String(localized: "Cancel")
            deleteButton.image = NSImage(
                systemSymbolName: "xmark.bin", accessibilityDescription: String(localized: "Cancel create icon")
            )
            deleteButton.bezelColor = NSColor.controlColor
            cancellable?.cancel()
            cancellable = nil
            controller = nil
            reset()
            setupCreateForm()
        }
    }
    private var cancellable: AnyCancellable?
    private var suggestionsWindowController = SuggestionsWindowController()
    private var suggestionsTextFieldDelegate = SuggestionsTextFieldDelegate()

    func applyLocalizedStrings() {
        publicLinkShareMenuItem.title = String(localized: "Public link share")
        userShareMenuItem.title = String(localized: "User share")
        groupShareMenuItem.title = String(localized: "Group share")
        emailShareMenuItem.title = String(localized: "Email share")
        federatedCloudShareMenuItem.title = String(localized: "Federated cloud share")
        teamShare.title = String(localized: "Team share")
        talkConversationShare.title = String(localized: "Talk conversation share")

        shareRecipientTextField.placeholderString = String(localized: "Share recipient")
        labelTextField.placeholderString = String(localized: "Share label")
        uploadEditPermissionCheckbox.title = String(localized: "Allow upload and editing")
        hideDownloadCheckbox.title = String(localized: "Hide download")
        passwordProtectCheckbox.title = String(localized: "Password protect")
        passwordSecureField.placeholderString = String(localized: "Enter a new password")
        expirationDateCheckbox.title = String(localized: "Expiration date")
        noteForRecipientCheckbox.title = String(localized: "Note for the recipient")
        noteTextField.placeholderString = String(localized: "Note for the recipient")

        deleteButton.title = String(localized: "Delete")
        saveButton.title = String(localized: "Save")
    }

    private func update() {
        guard let controller else {
            reset()
            setAllFields(enabled: false)
            saveButton.isEnabled = false
            deleteButton.isEnabled = false
            return
        }

        logger = FileProviderLogger(category: "ShareOptionsView", log: controller.log)
        let share = controller.share

        deleteButton.isEnabled = share.canDelete
        saveButton.isEnabled = share.canEdit

        setAllFields(enabled: share.canEdit)
        reset()

        shareRecipientTextField.stringValue = share.shareWithDisplayname
        labelTextField.stringValue = share.label
        uploadEditPermissionCheckbox.state = share.shareesCanEdit ? .on : .off
        hideDownloadCheckbox.state = share.hideDownload ? .on : .off
        passwordProtectCheckbox.state = share.password.isEmpty ? .off : .on
        passwordSecureField.isHidden = passwordProtectCheckbox.state == .off
        passwordSecureField.stringValue = share.password
        expirationDateCheckbox.state = share.expirationDate == nil ? .off : .on
        expirationDatePicker.isHidden = expirationDateCheckbox.state == .off
        expirationDatePicker.dateValue = share.expirationDate as? Date ?? Date()
        noteForRecipientCheckbox.state = share.note.isEmpty ? .off : .on
        noteTextField.isHidden = noteForRecipientCheckbox.state == .off
        noteForRecipientCheckbox.stringValue = share.note
    }

    private func reset() {
        shareRecipientTextField.stringValue = ""
        labelTextField.stringValue = ""
        uploadEditPermissionCheckbox.state = .off
        hideDownloadCheckbox.state = .off
        passwordProtectCheckbox.state = .off
        passwordSecureField.isHidden = true
        passwordSecureField.stringValue = ""
        expirationDateCheckbox.state = .off
        expirationDatePicker.isHidden = true
        expirationDatePicker.dateValue = NSDate.now
        expirationDatePicker.minDate = NSDate.now
        expirationDatePicker.maxDate = nil
        noteForRecipientCheckbox.state = .off
        noteTextField.isHidden = true
        noteTextField.stringValue = ""
    }

    private func setupCreateForm() {
        guard createMode else { return }

        setAllFields(enabled: true)

        let type = pickedShareType()
        shareRecipientTextField.isHidden = type == .publicLink

        if let caps = dataSource?.capabilities?.filesSharing {
            uploadEditPermissionCheckbox.state =
                caps.defaultPermissions & NKShare.PermissionValues.updateShare.rawValue != 0
                ? .on : .off

            switch type {
            case .publicLink:
                passwordProtectCheckbox.isHidden = false
                passwordProtectCheckbox.state = caps.publicLink?.passwordEnforced == true ? .on : .off
                passwordProtectCheckbox.isEnabled = caps.publicLink?.passwordEnforced == false
                expirationDateCheckbox.state = caps.publicLink?.expireDateEnforced == true ? .on : .off
                expirationDateCheckbox.isEnabled = caps.publicLink?.expireDateEnforced == false
                expirationDatePicker.dateValue = Date(
                    timeIntervalSinceNow: 
                        TimeInterval((caps.publicLink?.expireDateDays ?? 1) * 24 * 60 * 60)
                )
                if caps.publicLink?.expireDateEnforced == true {
                    expirationDatePicker.maxDate = expirationDatePicker.dateValue
                }
            case .email:
                passwordProtectCheckbox.isHidden = caps.email?.passwordEnabled == false
                passwordProtectCheckbox.state = caps.email?.passwordEnforced == true ? .on : .off
            default:
                passwordProtectCheckbox.isHidden = true
                passwordProtectCheckbox.state = .off
                break
            }
        }

        passwordSecureField.isHidden = passwordProtectCheckbox.state == .off
        expirationDatePicker.isHidden = expirationDateCheckbox.state == .off
    }

    private func setAllFields(enabled: Bool) {
        shareTypePicker.isEnabled = enabled
        shareRecipientTextField.isEnabled = enabled
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

    private func pickedShareType() -> NKShare.ShareType {
        let selectedShareTypeItem = shareTypePicker.selectedItem
        var selectedShareType = NKShare.ShareType.publicLink
        if selectedShareTypeItem == publicLinkShareMenuItem {
            selectedShareType = .publicLink
        } else if selectedShareTypeItem == userShareMenuItem {
            selectedShareType = .user
        } else if selectedShareTypeItem == groupShareMenuItem {
            selectedShareType = .group
        } else if selectedShareTypeItem == emailShareMenuItem {
            selectedShareType = .email
        } else if selectedShareTypeItem == federatedCloudShareMenuItem {
            selectedShareType = .federatedCloud
        } else if selectedShareTypeItem == teamShare {
            selectedShareType = .team
        } else if selectedShareTypeItem == talkConversationShare {
            selectedShareType = .talkConversation
        }
        return selectedShareType
    }

    @IBAction func shareTypePickerAction(_ sender: Any) {
        if createMode {
            setupCreateForm()
        }
    }

    @IBAction func passwordCheckboxAction(_ sender: Any) {
        passwordSecureField.isHidden = passwordProtectCheckbox.state == .off
    }

    @IBAction func expirationDateCheckboxAction(_ sender: Any) {
        expirationDatePicker.isHidden = expirationDateCheckbox.state == .off
    }

    @IBAction func noteForRecipientCheckboxAction(_ sender: Any) {
        noteTextField.isHidden = noteForRecipientCheckbox.state == .off
    }

    @IBAction func save(_ sender: Any) {
        Task { @MainActor in
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

            guard !createMode else {
                logger?.info("Creating new share!")

                guard let dataSource,
                      let account,
                      let itemServerRelativePath = dataSource.itemServerRelativePath
                else {
                    logger?.error("Cannot create new share due to missing data. dataSource: \(String(describing: self.dataSource)) account: \(self.account != nil) path: \(self.dataSource?.itemServerRelativePath ?? "")")
                    return
                }

                let selectedShareType = pickedShareType()
                let shareWith = shareRecipientTextField.stringValue

                var permissions = NKShare.PermissionValues.all.rawValue
                permissions = uploadAndEdit
                    ? permissions | NKShare.PermissionValues.updateShare.rawValue
                    : permissions & ~NKShare.PermissionValues.updateShare.rawValue

                setAllFields(enabled: false)
                deleteButton.isEnabled = false
                saveButton.isEnabled = false
                let error = await ShareController.create(
                    account: account,
                    kit: kit,
                    shareType: selectedShareType,
                    itemServerRelativePath: itemServerRelativePath,
                    shareWith: shareWith,
                    password: password,
                    expireDate: expireDate,
                    permissions: permissions,
                    note: note,
                    label: label,
                    hideDownload: hideDownload
                )
                if let error = error, error != .success {
                    dataSource.uiDelegate?.showError(String(localized: "Error creating: \(error.errorDescription)"))
                    setAllFields(enabled: true)
                } else {
                    dataSource.uiDelegate?.hideOptions(self)
                    await dataSource.reload()
                }
                return
            }

            logger?.info("Editing existing share!")

            guard let controller = controller else {
                logger?.error("No valid share controller, cannot edit share.")
                return
            }
            let share = controller.share
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
                setAllFields(enabled: true)
            } else {
                dataSource?.uiDelegate?.hideOptions(self)
                await dataSource?.reload()
            }
        }
    }

    @IBAction func delete(_ sender: Any) {
        Task { @MainActor in
            guard !createMode else {
                dataSource?.uiDelegate?.hideOptions(self)
                reset()
                return
            }

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

