//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import ArgumentParser
import Foundation

@main
struct TransifexStringCatalogSanitizer: ParsableCommand {
    @Argument(help: "The string catalog file to sanitize.")
    var input: String

    @Flag(name: .long, help: "Report what would change without writing to disk or creating a backup file.")
    var dryRun = false

    mutating func run() throws {
        let url = URL(fileURLWithPath: input).standardized

        guard FileManager.default.fileExists(atPath: url.path) else {
            throw TransifexStringCatalogSanitizerError.missingFile(url.path)
        }

        let data = try Data(contentsOf: url)

        guard var root = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw TransifexStringCatalogSanitizerError.jsonObject
        }

        guard let sourceLanguage = root["sourceLanguage"] as? String else {
            throw TransifexStringCatalogSanitizerError.missingSourceLanguage
        }

        guard var strings = root["strings"] as? [String: Any] else {
            throw TransifexStringCatalogSanitizerError.missingStrings
        }

        if dryRun {
            print("🔍 Dry run: no files will be written.\n")
        }

        try Sanitizer.sanitizeStrings(&strings, sourceLanguage: sourceLanguage)

        // Update the root with modified strings
        root["strings"] = strings

        // Write the processed data back to the original file
        let processedData = try JSONSerialization.data(withJSONObject: root, options: [.prettyPrinted, .sortedKeys])

        guard !dryRun else {
            print("\n🔍 Dry run complete. Nothing was written to disk.")
            return
        }

        // Preserve the pre-sanitize bytes so a bad run can be recovered from without
        // re-pulling from Transifex. This must be the last thing before the real write:
        // every guard/throw above has already succeeded by this point, so a crash during
        // parsing or sanitizing never creates (or clobbers) a backup for a write that
        // was never going to happen.
        try data.write(to: url.appendingPathExtension("bak"))

        try processedData.write(to: url)
    }
}
