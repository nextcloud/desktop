//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import ArgumentParser
import Foundation

@main
struct TransifexStringCatalogSanitizer: ParsableCommand {
    @Argument(help: "The string catalog file to sanitize.")
    var input: String
    
    mutating func run() throws {
        let url = URL(fileURLWithPath: input).standardized

        guard FileManager.default.fileExists(atPath: url.path) else {
            throw TransifexStringCatalogSanitizerError.missingFile(url.path)
        }

        let data = try Data(contentsOf: url)
        
        guard var root = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw TransifexStringCatalogSanitizerError.jsonObject
        }
        
        guard var strings = root["strings"] as? [String: Any] else {
            throw TransifexStringCatalogSanitizerError.missingStrings
        }
        
        try sanitizeStrings(&strings)
        
        // Update the root with modified strings
        root["strings"] = strings
        
        // Write the processed data back to the original file
        let processedData = try JSONSerialization.data(withJSONObject: root, options: [.prettyPrinted, .sortedKeys])
        try processedData.write(to: url)
    }
    
    private func sanitizeStrings(_ strings: inout [String: Any]) throws {
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
            
            try sanitizeLocalizations(&localizations)
            
            // Update the string with modified localizations
            string["localizations"] = localizations
            strings[key] = string
        }
    }
    
    private func sanitizeLocalizations(_ localizations: inout [String: Any]) throws {
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
