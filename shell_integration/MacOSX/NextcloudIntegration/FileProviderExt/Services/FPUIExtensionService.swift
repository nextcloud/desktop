//
//  FPUIExtensionCommunicationProtocol.swift
//  FileProviderExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import FileProvider
import NextcloudKit

let fpUiExtensionServiceName = NSFileProviderServiceName(
    "com.nextcloud.desktopclient.FPUIExtensionService"
)

@objc protocol FPUIExtensionService {
    func userAgent() async -> NSString?
    func credentials() async -> NSDictionary
    func itemServerPath(identifier: NSFileProviderItemIdentifier) async -> NSString?
}
