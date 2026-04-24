//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import Foundation
import os

///
/// Abstraction for the user defaults specific to file provider domains.
///
public struct FileProviderDomainDefaults {
    ///
    /// > Warning: Do not change the raw values of these keys, as they are used in UserDefaults. Any change would make the already stored value inaccessible and be like a reset.
    ///
    private enum ConfigKey: String {
        ///
        /// Obsolete and kept only for documentation to avoid future collisions.
        ///
        case trashDeletionEnabled
        case user
        case userId
        case serverUrl
        case debugLoggingEnabled
    }

    ///
    /// The file provider domain identifier which the current settings belong to.
    ///
    let identifier: NSFileProviderDomainIdentifier

    let logger: FileProviderLogger

    public init(identifier: NSFileProviderDomainIdentifier, log: any FileProviderLogging) {
        self.identifier = identifier
        logger = FileProviderLogger(category: "FileProviderDomainDefaults", log: log)
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
    public var serverUrl: String? {
        get {
            let identifier = identifier.rawValue

            if let value = internalConfig[ConfigKey.serverUrl.rawValue] as? String {
                logger.debug("Returning existing value \"\(value)\" for \"serverUrl\" for file provider domain \"\(identifier)\".")
                return value
            } else {
                logger.debug("No existing value for \"serverUrl\" for file provider domain \"\(identifier)\" found.")
                return nil
            }
        }

        set {
            let identifier = identifier.rawValue

            if newValue == nil {
                logger.debug("Removing key \"serverUrl\" for file provider domain \"\(identifier)\" because the new value is nil.")
                internalConfig.removeValue(forKey: ConfigKey.serverUrl.rawValue)
            } else if newValue == "" {
                logger.error("Ignoring new value for \"serverUrl\" because it is an empty string for file provider domain \"\(identifier)\"!")
            } else {
                internalConfig[ConfigKey.serverUrl.rawValue] = newValue
            }
        }
    }

    ///
    /// The user name associated with the domain.
    ///
    public var user: String? {
        get {
            let identifier = identifier.rawValue

            if let value = internalConfig[ConfigKey.user.rawValue] as? String {
                logger.debug("Returning existing value \"\(value)\" for \"user\" for file provider domain \"\(identifier)\".")
                return value
            } else {
                logger.debug("No existing value for \"user\" for file provider domain \"\(identifier)\" found.")
                return nil
            }
        }

        set {
            let identifier = identifier.rawValue

            if newValue == nil {
                logger.debug("Removing key \"user\" for file provider domain \"\(identifier)\" because the new value is nil.")
                internalConfig.removeValue(forKey: ConfigKey.user.rawValue)
            } else if newValue == "" {
                logger.error("Ignoring new value for \"user\" because it is an empty string for file provider domain \"\(identifier)\"!")
            } else {
                internalConfig[ConfigKey.user.rawValue] = newValue
            }
        }
    }

    ///
    /// The full user identifier associated with the domain.
    ///
    public var userId: String? {
        get {
            let identifier = identifier.rawValue

            if let value = internalConfig[ConfigKey.userId.rawValue] as? String {
                logger.debug("Returning existing value \"\(value)\" for \"userId\" for file provider domain \"\(identifier)\".")
                return value
            } else {
                logger.debug("No existing value for \"userId\" for file provider domain \"\(identifier)\" found.")
                return nil
            }
        }

        set {
            let identifier = identifier.rawValue

            if newValue == nil {
                logger.debug("Removing key \"userId\" for file provider domain \"\(identifier)\" because the new value is nil.")
                internalConfig.removeValue(forKey: ConfigKey.userId.rawValue)
            } else if newValue == "" {
                logger.error("Ignoring new value for \"userId\" because it is an empty string for file provider domain \"\(identifier)\"!")
            } else {
                internalConfig[ConfigKey.userId.rawValue] = newValue
            }
        }
    }

    ///
    /// Controls whether `.debug`-level messages are included in logging output.
    ///
    /// Unlike the other properties on this type, this value is **process-global and shared across all file provider domains**.
    /// It is stored at the top level of `UserDefaults.standard`, bypassing the per-domain `internalConfig` dictionary, because `FileProviderLog` is instantiated once per process and administrators configure it with a single `defaults write` command per extension bundle.
    ///
    /// When unset, debug logging is enabled in DEBUG builds and disabled in release builds, matching the previous compile-time behavior.
    /// Administrators can override at runtime for troubleshooting.
    ///
    public var debugLoggingEnabled: Bool? {
        get {
            let key = ConfigKey.debugLoggingEnabled.rawValue

            if let value = UserDefaults.standard.object(forKey: key) as? Bool {
                logger.debug("Returning existing value \"\(value)\" for \"debugLoggingEnabled\" (process-global).")
                return value
            } else {
                logger.debug("No existing value for \"debugLoggingEnabled\" (process-global) found.")
                return nil
            }
        }

        set {
            let key = ConfigKey.debugLoggingEnabled.rawValue

            if let newValue {
                UserDefaults.standard.set(newValue, forKey: key)
            } else {
                logger.debug("Removing key \"debugLoggingEnabled\" (process-global) because the new value is nil.")
                UserDefaults.standard.removeObject(forKey: key)
            }
        }
    }
}
