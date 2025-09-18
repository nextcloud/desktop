//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import os

///
/// Abstraction for the user defaults specific to file provider domains.
///
struct FileProviderDomainDefaults {
    ///
    /// > Warning: Do not change the raw values of these keys, as they are used in UserDefaults. Any change would make the already stored value inaccessible and be like a reset.
    ///
    private enum ConfigKey: String {
        case trashDeletionEnabled
        case user
        case userId
        case serverUrl
    }

    ///
    /// The file provider domain identifier which the current settings belong to.
    ///
    let identifier: NSFileProviderDomainIdentifier

    init(identifier: NSFileProviderDomainIdentifier) {
        self.identifier = identifier
    }

    ///
    /// Either fetch the existing settings for the given domain identifier or create a new empty dictionary.
    ///
    private var internalConfig: [String: Any] {
        get {
            let defaults = UserDefaults.standard

            if let settings = defaults.dictionary(forKey: identifier.rawValue) {
                return settings
            }

            let dictionary: [String: Any] = [:]
            defaults.setValue(dictionary, forKey: identifier.rawValue)

            return dictionary
        }

        set {
            UserDefaults.standard.setValue(newValue, forKey: identifier.rawValue)
        }
    }

    ///
    /// The address of the server to connect to.
    ///
    var serverUrl: String? {
        get {
            let identifier = self.identifier.rawValue

            if let value = internalConfig[ConfigKey.serverUrl.rawValue] as? String {
                Logger.fileProviderDomainDefaults.debug("Returning existing value \"\(value)\" for \"serverUrl\" for file provider domain \"\(identifier)\".")
                return value
            } else {
                Logger.fileProviderDomainDefaults.debug("No existing value for \"serverUrl\" for file provider domain \"\(identifier)\" found.")
                return nil
            }
        }

        set {
            let identifier = self.identifier.rawValue

            if newValue == nil {
                Logger.fileProviderDomainDefaults.debug("Removing key \"serverUrl\" for file provider domain \"\(identifier)\" because the new value is nil.")
                internalConfig.removeValue(forKey: ConfigKey.serverUrl.rawValue)
            } else if newValue == "" {
                Logger.fileProviderDomainDefaults.error("Ignoring new value for \"serverUrl\" because it is an empty string for file provider domain \"\(identifier)\"!")
            } else {
                internalConfig[ConfigKey.serverUrl.rawValue] = newValue
            }
        }
    }

    ///
    /// Whether trash deletion is enabled or not.
    ///
    var trashDeletionEnabled: Bool {
        get {
            let identifier = self.identifier.rawValue

            if let value = internalConfig[ConfigKey.trashDeletionEnabled.rawValue] as? Bool {
                Logger.fileProviderDomainDefaults.debug("Returning existing value \"\(value)\" for \"trashDeletionEnabled\" for file provider domain \"\(identifier)\".")
                return value
            } else {
                return false
            }
        }

        set {
            let identifier = self.identifier.rawValue

            Logger.fileProviderDomainDefaults.error("Setting value \"\(newValue)\" for \"trashDeletionEnabled\" for file provider domain \"\(identifier)\".")
            internalConfig[ConfigKey.trashDeletionEnabled.rawValue] = newValue
        }
    }

    ///
    /// The user name associated with the domain.
    ///
    var user: String? {
        get {
            let identifier = self.identifier.rawValue

            if let value = internalConfig[ConfigKey.user.rawValue] as? String {
                Logger.fileProviderDomainDefaults.debug("Returning existing value \"\(value)\" for \"user\" for file provider domain \"\(identifier)\".")
                return value
            } else {
                Logger.fileProviderDomainDefaults.debug("No existing value for \"user\" for file provider domain \"\(identifier)\" found.")
                return nil
            }
        }

        set {
            let identifier = self.identifier.rawValue

            if newValue == nil {
                Logger.fileProviderDomainDefaults.debug("Removing key \"user\" for file provider domain \"\(identifier)\" because the new value is nil.")
                internalConfig.removeValue(forKey: ConfigKey.user.rawValue)
            } else if newValue == "" {
                Logger.fileProviderDomainDefaults.error("Ignoring new value for \"user\" because it is an empty string for file provider domain \"\(identifier)\"!")
            } else {
                internalConfig[ConfigKey.user.rawValue] = newValue
            }
        }
    }

    ///
    /// The full user identifier associated with the domain.
    ///
    var userId: String? {
        get {
            let identifier = self.identifier.rawValue

            if let value = internalConfig[ConfigKey.userId.rawValue] as? String {
                Logger.fileProviderDomainDefaults.debug("Returning existing value \"\(value)\" for \"userId\" for file provider domain \"\(identifier)\".")
                return value
            } else {
                Logger.fileProviderDomainDefaults.debug("No existing value for \"userId\" for file provider domain \"\(identifier)\" found.")
                return nil
            }
        }

        set {
            let identifier = self.identifier.rawValue

            if newValue == nil {
                Logger.fileProviderDomainDefaults.debug("Removing key \"userId\" for file provider domain \"\(identifier)\" because the new value is nil.")
                internalConfig.removeValue(forKey: ConfigKey.userId.rawValue)
            } else if newValue == "" {
                Logger.fileProviderDomainDefaults.error("Ignoring new value for \"userId\" because it is an empty string for file provider domain \"\(identifier)\"!")
            } else {
                internalConfig[ConfigKey.userId.rawValue] = newValue
            }
        }
    }

    ///
    /// Whether a value for `trashDeletionEnabled` has been explicitly set.
    ///
    lazy var trashDeletionSet = internalConfig[ConfigKey.trashDeletionEnabled.rawValue] != nil
}
