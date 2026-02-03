//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import NextcloudKit

///
/// The descriptive identifier for the service exposed at certain locations.
///
/// This does not need to be branded because it is scoped by the app-specific file provider domains in the file system already.
///
let fpUiExtensionServiceName = NSFileProviderServiceName("com.nextcloud.desktopclient.FPUIExtensionService")

///
/// The requirements of the service exposed and dedicated to the file provider user interface extension.
///
@objc protocol FPUIExtensionService {
    ///
    /// Request (re)authentication with the available credentials.
    ///
    /// - Returns: An error in case of failure, otherwise `nil`.
    ///
    func authenticate() async -> NSError?

    ///
    /// Fetch the user agent used by the underlying NextcloudKit.
    ///
    func userAgent() async -> NSString?

    ///
    /// Fetch the credentials used by the file provider extension.
    ///
    func credentials() async -> NSDictionary

    ///
    /// Get a server URL for the given local file provider item.
    ///
    func itemServerPath(identifier: NSFileProviderItemIdentifier) async -> NSString?
}
