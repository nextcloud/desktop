//
//  FileProviderConfig.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 5/2/24.
//

import FileProvider
import Foundation

struct FileProviderConfig {
    enum FileProviderConfigKey: String {
        case fastEnumerationEnabled = "fastEnumerationEnabled"
    }

    let domainIdentifier: NSFileProviderDomainIdentifier

}
