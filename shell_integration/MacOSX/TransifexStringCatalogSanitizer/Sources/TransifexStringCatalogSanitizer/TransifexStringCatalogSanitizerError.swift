//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

enum TransifexStringCatalogSanitizerError: Error {
    case jsonObject
    case missingFile(String)
    case missingLocalization(String)
    case missingLocalizations(String)
    case missingString(String)
    case missingStrings
    case missingStringUnit(String)
    case missingValue
}
extension TransifexStringCatalogSanitizerError: LocalizedError {
    var errorDescription: String? {
        switch self {
        case .jsonObject:
            return "The string catalog data is not a valid JSON object."
        case let .missingFile(path):
            return "The string catalog file could not be found: \(path)"
        case let .missingLocalization(localeCode):
            return "The expected localization dictionary for locale code \"\(localeCode)\" was either not found or could not be casted to the expected type."
        case let .missingLocalizations(key):
            return "The localizations object of a string object with the key \"\(key)\" was either not found or could not be casted to the expected type."
        case let .missingString(key):
            return "An expected string dictionary with the key \"\(key)\" was either not found or could not be casted to the expected type."
        case .missingStrings:
            return "The string catalog is missing the strings dictionary."
        case let .missingStringUnit(localeCode):
            return "The expected string unit dictionary for locale code \"\(localeCode)\" was either not found or could not be casted to the expected type."
        case .missingValue:
            return "A string value is missing from a string unit."
        }
    }
}

