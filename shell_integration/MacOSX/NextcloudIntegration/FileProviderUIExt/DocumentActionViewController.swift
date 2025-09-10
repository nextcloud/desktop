//
//  DocumentActionViewController.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import FileProviderUI
import OSLog

class DocumentActionViewController: FPUIActionExtensionViewController {
    var domain: NSFileProviderDomain {
        guard let identifier = extensionContext.domainIdentifier else {
            fatalError("not expected to be called with default domain")
        }
        return NSFileProviderDomain(
            identifier: NSFileProviderDomainIdentifier(rawValue: identifier.rawValue),
            displayName: ""
        )
    }

    func prepare(childViewController: NSViewController) {
        addChild(childViewController)
        view.addSubview(childViewController.view)

        NSLayoutConstraint.activate([
            view.leadingAnchor.constraint(equalTo: childViewController.view.leadingAnchor),
            view.trailingAnchor.constraint(equalTo: childViewController.view.trailingAnchor),
            view.topAnchor.constraint(equalTo: childViewController.view.topAnchor),
            view.bottomAnchor.constraint(equalTo: childViewController.view.bottomAnchor)
        ])
    }

    override func prepare(
        forAction actionIdentifier: String, itemIdentifiers: [NSFileProviderItemIdentifier]
    ) {
        Logger.actionViewController.info("Preparing action: \(actionIdentifier, privacy: .public)")

        switch (actionIdentifier) {
        case "com.nextcloud.desktopclient.FileProviderUIExt.ShareAction":
            prepare(childViewController: ShareViewController(itemIdentifiers))
        case "com.nextcloud.desktopclient.FileProviderUIExt.LockFileAction":
            prepare(childViewController: LockViewController(itemIdentifiers, locking: true))
        case "com.nextcloud.desktopclient.FileProviderUIExt.UnlockFileAction":
            prepare(childViewController: LockViewController(itemIdentifiers, locking: false))
        case "com.nextcloud.desktopclient.FileProviderUIExt.EvictAction":
            evict(itemsWithIdentifiers: itemIdentifiers, inDomain: domain);
            extensionContext.completeRequest();
        default:
            return
        }
    }

    override func prepare(forError error: Error) {
        Logger.actionViewController.info("Preparing for error: \(error.localizedDescription, privacy: .public)")

        let storyboard = NSStoryboard(name: "Authentication", bundle: Bundle(for: type(of: self)))
        let viewController = storyboard.instantiateInitialController() as! NSViewController

        prepare(childViewController: viewController)
    }

    override public func loadView() {
        self.view = NSView()
    }
}
