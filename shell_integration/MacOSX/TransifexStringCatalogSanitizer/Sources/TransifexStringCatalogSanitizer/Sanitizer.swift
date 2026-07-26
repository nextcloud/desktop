//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Pure sanitization logic for Xcode string catalogs (`.xcstrings`) pulled from Transifex.
/// Contains no file I/O, no CLI argument handling, and no backup/dry-run concerns — those
/// live in `TransifexStringCatalogSanitizer` (the `ParsableCommand`). Kept separate so it
/// can be exercised directly by unit tests via `@testable import`, without touching disk
/// or invoking ArgumentParser.
///
enum Sanitizer {
    ///
    /// Sanitize the string keys.
    ///
    static func sanitizeStrings(_ strings: inout [String: Any], sourceLanguage: String) throws {
        for key in strings.keys.sorted() {
            print("💬 \"\(key)\"")

            guard var string = strings[key] as? [String: Any] else {
                throw TransifexStringCatalogSanitizerError.missingString(key)
            }

            let extractionState = string["extractionState"] as? String

            if extractionState == nil && string["localizations"] == nil {
                print("\t🆕 This appears to be new because neither extraction state nor localization objects found.")
                continue
            }

            if extractionState == "stale" {
                print("\t❌ This is removed because it has been marked as stale.")
                strings[key] = nil
                continue
            }

            guard var localizations = string["localizations"] as? [String: Any] else {
                throw TransifexStringCatalogSanitizerError.missingLocalizations(key)
            }

            try sanitizeLocalizations(&localizations, key: key, sourceLanguage: sourceLanguage)

            // Update the string with modified localizations
            string["localizations"] = localizations
            strings[key] = string
        }
    }

    ///
    /// Sanitize the individual localizations of a key.
    ///
    static func sanitizeLocalizations(_ localizations: inout [String: Any], key: String, sourceLanguage: String) throws {
        var localizationsToRemove: [String] = []

        for localeCode in localizations.keys.sorted() {
            guard let localization = localizations[localeCode] as? [String: Any] else {
                throw TransifexStringCatalogSanitizerError.missingLocalization(localeCode)
            }

            guard let stringUnit = localization["stringUnit"] as? [String: Any] else {
                throw TransifexStringCatalogSanitizerError.missingStringUnit(localeCode)
            }

            guard let value = stringUnit["value"] as? String else {
                throw TransifexStringCatalogSanitizerError.missingValue
            }

            if value.isEmpty {
                print("\t❌ \(localeCode): empty, will be removed")
                localizationsToRemove.append(localeCode)
            } else if value == key && localeCode.hasPrefix(sourceLanguage) == false {
                print("\t❌ \(localeCode): same as key, will be removed")
                localizationsToRemove.append(localeCode)
            } else {
                print("\t✅ \(localeCode): \"\(value)\"")
            }
        }

        // Remove empty localizations
        for localeCode in localizationsToRemove {
            localizations.removeValue(forKey: localeCode)
        }
    }
}
