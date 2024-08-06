//
//  FPUIExtensionCommunicationProtocol.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 21/2/24.
//

import FileProvider
import NextcloudKit

let fpUiExtensionServiceName = NSFileProviderServiceName(
    "com.nextcloud.desktopclient.FPUIExtensionService"
)

@objc protocol FPUIExtensionService {
    func credentials() async -> NSDictionary
    func itemServerPath(identifier: NSFileProviderItemIdentifier) async -> NSString?
}
