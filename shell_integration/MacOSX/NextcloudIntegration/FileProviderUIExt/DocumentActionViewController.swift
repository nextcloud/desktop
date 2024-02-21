//
//  DocumentActionViewController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 20/2/24.
//

import FileProviderUI
import OSLog

class DocumentActionViewController: FPUIActionExtensionViewController {
    override func prepare(
        forAction actionIdentifier: String, itemIdentifiers: [NSFileProviderItemIdentifier]
    ) {
        Logger.actionViewController.info("Preparing for action: \(actionIdentifier)")
        let shareViewController = ShareViewController()
        addChild(shareViewController)
        view.addSubview(shareViewController.view)

        NSLayoutConstraint.activate([
            view.leadingAnchor.constraint(equalTo: shareViewController.view.leadingAnchor),
            view.trailingAnchor.constraint(equalTo: shareViewController.view.trailingAnchor),
            view.topAnchor.constraint(equalTo: shareViewController.view.topAnchor),
            view.bottomAnchor.constraint(equalTo: shareViewController.view.bottomAnchor)
        ])
    }
    
    override func prepare(forError error: Error) {
        Logger.actionViewController.info("Preparing for error: \(error.localizedDescription)")
    }

    override public func loadView() {
        self.view = NSView()
    }

    @IBAction func doneButtonTapped(_ sender: Any) {
        // Perform the action and call the completion block. If an unrecoverable error occurs you must still call the completion block with an error. Use the error code FPUIExtensionErrorCode.failed to signal the failure.
        extensionContext.completeRequest()
    }
    
    @IBAction func cancelButtonTapped(_ sender: Any) {
        extensionContext.cancelRequest(withError: NSError(domain: FPUIErrorDomain, code: Int(FPUIExtensionErrorCode.userCancelled.rawValue), userInfo: nil))
    }
    
}

