//
//  FileProviderConfig.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 5/2/24.
//

import FileProvider
import Foundation

struct FileProviderConfig {
    private enum ConfigKey: String {
        case fastEnumerationEnabled = "fastEnumerationEnabled"
    }

    let domainIdentifier: NSFileProviderDomainIdentifier

    private var internalConfig: [String: Any] {
        get {
            let defaults = UserDefaults.standard
            if let settings = defaults.dictionary(forKey: domainIdentifier.rawValue) {
                return settings
            }
            let dictionary: [String: Any] = [:]
            defaults.setValue(dictionary, forKey: domainIdentifier.rawValue)
            return dictionary
        }
        set {
            let defaults = UserDefaults.standard
            defaults.setValue(newValue, forKey: domainIdentifier.rawValue)
        }
    }

    var fastEnumerationEnabled: Bool {
        get { internalConfig[ConfigKey.fastEnumerationEnabled.rawValue] as? Bool ?? true }
        set { internalConfig[ConfigKey.fastEnumerationEnabled.rawValue] = newValue }
    }

    lazy var fastEnumerationSet = internalConfig[ConfigKey.fastEnumerationEnabled.rawValue] != nil
}
