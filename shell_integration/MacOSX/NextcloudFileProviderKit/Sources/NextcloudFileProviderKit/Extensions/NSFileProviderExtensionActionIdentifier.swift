//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

extension NSFileProviderExtensionActionIdentifier {
    ///
    /// Shared custom action identifier prefix.
    ///
    private static var prefix: String {
        "com.nextcloud.desktopclient.FileProviderExt."
    }

    ///
    /// Custom action to evict a materialized item.
    ///
    /// The raw value must be maintained manually and redundantly in the custom actions of the file provider extension target declared at build time.
    ///
    static var evict: NSFileProviderExtensionActionIdentifier {
        NSFileProviderExtensionActionIdentifier("\(prefix)EvictAction")
    }

    ///
    /// Custom action to change the content policy for an item to allow automatic eviction by the system.
    ///
    /// The raw value must be maintained manually and redundantly in the custom actions of the file provider extension target declared at build time.
    ///
    static var evictAutomatically: NSFileProviderExtensionActionIdentifier {
        NSFileProviderExtensionActionIdentifier("\(prefix)AutoEvictAction")
    }

    ///
    /// Custom action for revelation of the file actions defined by the Nextcloud server.
    ///
    /// The raw value must be maintained manually and redundantly in the custom actions of the file provider extension target declared at build time.
    ///
    static var fileActions: NSFileProviderExtensionActionIdentifier {
        NSFileProviderExtensionActionIdentifier("\(prefix)FileActionsAction")
    }

    ///
    /// Custom action to change the content policy for an item to prevent automatic eviction by the system.
    ///
    /// The raw value must be maintained manually and redundantly in the custom actions of the file provider extension target declared at build time.
    ///
    static var keepDownloaded: NSFileProviderExtensionActionIdentifier {
        NSFileProviderExtensionActionIdentifier("\(prefix)KeepDownloadedAction")
    }

    ///
    /// Custom action to reveal the selected item in the web user interface of Nextcloud server.
    ///
    /// The raw value must be maintained manually and redundantly in the custom actions of the file provider extension target declared at build time.
    ///
    static var openInBrowser: NSFileProviderExtensionActionIdentifier {
        NSFileProviderExtensionActionIdentifier("\(prefix)OpenInBrowserAction")
    }
}
