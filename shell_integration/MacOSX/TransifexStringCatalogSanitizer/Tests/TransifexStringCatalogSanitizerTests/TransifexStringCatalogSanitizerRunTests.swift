//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import ArgumentParser
import Foundation
import Testing
@testable import TransifexStringCatalogSanitizer

struct TransifexStringCatalogSanitizerRunTests {
    private let sampleCatalog = """
    { "sourceLanguage": "en", "version": "1.0", "strings": {
        "Old": { "extractionState": "stale", "localizations": {
            "en": {"stringUnit": {"state": "translated", "value": "Old"}}
        }},
        "Welcome": { "extractionState": "manual", "localizations": {
            "en": {"stringUnit": {"state": "translated", "value": "Welcome"}},
            "de": {"stringUnit": {"state": "translated", "value": "Willkommen"}}
        }}
    }}
    """

    private func makeTempFile(_ contents: String) throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString)
            .appendingPathExtension("xcstrings")
        try Data(contents.utf8).write(to: url)
        return url
    }

    private func removeTempFile(_ url: URL) {
        try? FileManager.default.removeItem(at: url)
        try? FileManager.default.removeItem(at: url.appendingPathExtension("bak"))
    }

    @Test func missingFile_throwsMissingFile() throws {
        var command = try TransifexStringCatalogSanitizer.parse(["/nonexistent/path/x.xcstrings"])

        #expect {
            try command.run()
        } throws: { error in
            guard case TransifexStringCatalogSanitizerError.missingFile = error else { return false }
            return true
        }
    }

    @Test func nonJSONContent_throwsAnError() throws {
        let url = try makeTempFile("this is not json")
        defer { removeTempFile(url) }
        var command = try TransifexStringCatalogSanitizer.parse([url.path])

        // Syntactically invalid JSON fails inside JSONSerialization.jsonObject itself,
        // before the `guard ... as?` cast runs — so this is a Foundation decoding error,
        // not TransifexStringCatalogSanitizerError.jsonObject.
        #expect(throws: (any Error).self) {
            try command.run()
        }
    }

    @Test func jsonArrayInsteadOfObject_throwsJsonObjectError() throws {
        let url = try makeTempFile("[]")
        defer { removeTempFile(url) }
        var command = try TransifexStringCatalogSanitizer.parse([url.path])

        #expect {
            try command.run()
        } throws: { error in
            guard case TransifexStringCatalogSanitizerError.jsonObject = error else { return false }
            return true
        }
    }

    @Test func missingSourceLanguage_throwsMissingSourceLanguage() throws {
        let url = try makeTempFile(#"{"strings": {}}"#)
        defer { removeTempFile(url) }
        var command = try TransifexStringCatalogSanitizer.parse([url.path])

        #expect {
            try command.run()
        } throws: { error in
            guard case TransifexStringCatalogSanitizerError.missingSourceLanguage = error else { return false }
            return true
        }
    }

    @Test func missingStrings_throwsMissingStrings() throws {
        let url = try makeTempFile(#"{"sourceLanguage": "en"}"#)
        defer { removeTempFile(url) }
        var command = try TransifexStringCatalogSanitizer.parse([url.path])

        #expect {
            try command.run()
        } throws: { error in
            guard case TransifexStringCatalogSanitizerError.missingStrings = error else { return false }
            return true
        }
    }

    @Test func realRun_writesBackupAndSanitizesFile() throws {
        let url = try makeTempFile(sampleCatalog)
        defer { removeTempFile(url) }

        var command = try TransifexStringCatalogSanitizer.parse([url.path])
        try command.run()

        let backupURL = url.appendingPathExtension("bak")
        #expect(FileManager.default.fileExists(atPath: backupURL.path))
        #expect(try Data(contentsOf: backupURL) == Data(sampleCatalog.utf8))

        let root = try #require(try JSONSerialization.jsonObject(with: Data(contentsOf: url)) as? [String: Any])
        let strings = try #require(root["strings"] as? [String: Any])
        #expect(strings["Old"] == nil)
        #expect(strings["Welcome"] != nil)
    }

    @Test func dryRun_doesNotWriteOrBackup() throws {
        let url = try makeTempFile(sampleCatalog)
        defer { removeTempFile(url) }

        var command = try TransifexStringCatalogSanitizer.parse(["--dry-run", url.path])
        try command.run()

        #expect(try Data(contentsOf: url) == Data(sampleCatalog.utf8))
        #expect(!FileManager.default.fileExists(atPath: url.appendingPathExtension("bak").path))
    }

    @Test func realRun_overwritesPreviousBackup() throws {
        let url = try makeTempFile(sampleCatalog)
        defer { removeTempFile(url) }
        let backupURL = url.appendingPathExtension("bak")
        try Data("stale sentinel backup content".utf8).write(to: backupURL)

        var command = try TransifexStringCatalogSanitizer.parse([url.path])
        try command.run()

        #expect(try Data(contentsOf: backupURL) == Data(sampleCatalog.utf8))
    }
}
