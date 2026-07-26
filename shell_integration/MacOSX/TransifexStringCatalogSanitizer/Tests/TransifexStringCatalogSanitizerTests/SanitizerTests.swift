//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import Testing
@testable import TransifexStringCatalogSanitizer

struct SanitizerTests {
    private func makeDictionary(_ json: String) throws -> [String: Any] {
        try #require(try JSONSerialization.jsonObject(with: Data(json.utf8)) as? [String: Any])
    }

    // MARK: - sanitizeStrings

    @Test func staleExtractionState_removesWholeKey() throws {
        var strings = try makeDictionary("""
        {
            "Hello": {
                "extractionState": "stale",
                "localizations": {
                    "en": {"stringUnit": {"state": "translated", "value": "Hello"}}
                }
            }
        }
        """)

        try Sanitizer.sanitizeStrings(&strings, sourceLanguage: "en")

        #expect(strings["Hello"] == nil)
    }

    @Test func newKey_noExtractionStateNoLocalizations_passesThroughUnchanged() throws {
        var strings = try makeDictionary("""
        {
            "Brand New": {
                "comment": "Just extracted, not yet pulled from Transifex."
            }
        }
        """)

        try Sanitizer.sanitizeStrings(&strings, sourceLanguage: "en")

        let entry = try #require(strings["Brand New"] as? [String: Any])
        #expect(entry["comment"] as? String == "Just extracted, not yet pulled from Transifex.")
        #expect(entry["localizations"] == nil)
    }

    @Test func missingString_throwsMissingString() throws {
        var strings = try makeDictionary("""
        { "Broken": "not a dictionary" }
        """)

        #expect {
            try Sanitizer.sanitizeStrings(&strings, sourceLanguage: "en")
        } throws: { error in
            guard case TransifexStringCatalogSanitizerError.missingString(let key) = error else { return false }
            return key == "Broken"
        }
    }

    @Test func missingLocalizations_throwsMissingLocalizations() throws {
        var strings = try makeDictionary("""
        { "Untranslated": {"extractionState": "manual"} }
        """)

        #expect {
            try Sanitizer.sanitizeStrings(&strings, sourceLanguage: "en")
        } throws: { error in
            guard case TransifexStringCatalogSanitizerError.missingLocalizations(let key) = error else { return false }
            return key == "Untranslated"
        }
    }

    // MARK: - sanitizeLocalizations

    @Test func emptyLocalizationValue_isRemoved() throws {
        var localizations = try makeDictionary("""
        {
            "en": {"stringUnit": {"state": "translated", "value": "Welcome"}},
            "de": {"stringUnit": {"state": "translated", "value": ""}}
        }
        """)

        try Sanitizer.sanitizeLocalizations(&localizations, key: "Welcome", sourceLanguage: "en")

        #expect(localizations["de"] == nil)
        #expect(localizations["en"] != nil)
    }

    @Test func nonSourceLocaleValueEqualsKey_isRemoved() throws {
        var localizations = try makeDictionary("""
        {
            "de": {"stringUnit": {"state": "translated", "value": "Welcome"}}
        }
        """)

        try Sanitizer.sanitizeLocalizations(&localizations, key: "Welcome", sourceLanguage: "en")

        #expect(localizations["de"] == nil)
    }

    @Test(arguments: ["en", "en_GB", "en_US"])
    func sourceLanguagePrefixedLocales_areExempt(locale: String) throws {
        var localizations: [String: Any] = [
            locale: ["stringUnit": ["state": "translated", "value": "Settings"]],
        ]

        try Sanitizer.sanitizeLocalizations(&localizations, key: "Settings", sourceLanguage: "en")

        #expect(localizations[locale] != nil)
    }

    @Test func valueDifferentFromKey_isKept() throws {
        var localizations = try makeDictionary("""
        { "de": {"stringUnit": {"state": "translated", "value": "Willkommen"}} }
        """)

        try Sanitizer.sanitizeLocalizations(&localizations, key: "Welcome", sourceLanguage: "en")

        let localization = try #require(localizations["de"] as? [String: Any])
        let stringUnit = try #require(localization["stringUnit"] as? [String: Any])
        #expect(stringUnit["value"] as? String == "Willkommen")
    }

    @Test func missingLocalization_throwsMissingLocalization() throws {
        var localizations: [String: Any] = ["de": "unexpectedly not an object"]

        #expect {
            try Sanitizer.sanitizeLocalizations(&localizations, key: "AnyKey", sourceLanguage: "en")
        } throws: { error in
            guard case TransifexStringCatalogSanitizerError.missingLocalization(let localeCode) = error else { return false }
            return localeCode == "de"
        }
    }

    @Test func missingStringUnit_throwsMissingStringUnit() throws {
        var localizations = try makeDictionary("""
        { "de": {"notStringUnit": {}} }
        """)

        #expect {
            try Sanitizer.sanitizeLocalizations(&localizations, key: "AnyKey", sourceLanguage: "en")
        } throws: { error in
            guard case TransifexStringCatalogSanitizerError.missingStringUnit(let localeCode) = error else { return false }
            return localeCode == "de"
        }
    }

    @Test func missingValue_throwsMissingValue() throws {
        var localizations = try makeDictionary("""
        { "de": {"stringUnit": {"state": "translated"}} }
        """)

        #expect {
            try Sanitizer.sanitizeLocalizations(&localizations, key: "AnyKey", sourceLanguage: "en")
        } throws: { error in
            guard case TransifexStringCatalogSanitizerError.missingValue = error else { return false }
            return true
        }
    }
}
