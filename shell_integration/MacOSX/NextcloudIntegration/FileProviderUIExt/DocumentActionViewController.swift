//
//  DocumentActionViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 20/2/24.
//

import FileProviderUI

class DocumentActionViewController: FPUIActionExtensionViewController {
    
    @IBOutlet weak var identifierLabel: NSTextField!
    @IBOutlet weak var actionTypeLabel: NSTextField!

    override func prepare(forAction actionIdentifier: String, itemIdentifiers: [NSFileProviderItemIdentifier]) {
        identifierLabel?.stringValue = actionIdentifier
        actionTypeLabel?.stringValue = "Custom action"
    }
    
    override func prepare(forError error: Error) {
        identifierLabel?.stringValue = error.localizedDescription
        actionTypeLabel?.stringValue = "Authenticate"
    }

    @IBAction func doneButtonTapped(_ sender: Any) {
        // Perform the action and call the completion block. If an unrecoverable error occurs you must still call the completion block with an error. Use the error code FPUIExtensionErrorCode.failed to signal the failure.
        extensionContext.completeRequest()
    }
    
    @IBAction func cancelButtonTapped(_ sender: Any) {
        extensionContext.cancelRequest(withError: NSError(domain: FPUIErrorDomain, code: Int(FPUIExtensionErrorCode.userCancelled.rawValue), userInfo: nil))
    }
    
}

