//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import Testing

/// Guards the Finder context menu labels declared in the `Info.plist` files of the file provider extensions (nextcloud/desktop#10525).
///
/// The system looks up the label in the extension's string catalog by using the label from `Info.plist` as the key.
/// Changing only one of them silently drops all translations, so these tests read the real files of the Xcode project.
struct ContextMenuLabelsTests {
    /// The Info.plist and string catalog of one extension in the Xcode project.
    private struct ExtensionResources {
        let name: String
        let infoPlist: [String: Any]
        let catalogKeys: [String: [String: Any]]
    }

    /// Words which stay lowercase in title-style capitalization unless they are the first word.
    private static let lowercaseWords: Set<String> = [
        "a", "an", "the", "and", "but", "or", "nor", "for", "in", "on", "at", "to", "of", "by"
    ]

    private static let extensionNames = ["FileProviderExt", "FileProviderUIExt"]

    private static func integrationDirectory() -> URL {
        URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent() // NextcloudFileProviderKitTests
            .deletingLastPathComponent() // Tests
            .deletingLastPathComponent() // NextcloudFileProviderKit
            .deletingLastPathComponent() // MacOSX
            .appendingPathComponent("NextcloudIntegration", isDirectory: true)
    }

    private static func loadResources(of name: String) throws -> ExtensionResources {
        let directory = integrationDirectory().appendingPathComponent(name, isDirectory: true)

        let plistData = try Data(contentsOf: directory.appendingPathComponent("Info.plist"))
        let plist = try #require(
            try PropertyListSerialization.propertyList(from: plistData, format: nil) as? [String: Any]
        )

        let catalogData = try Data(contentsOf: directory.appendingPathComponent("Localizable.xcstrings"))
        let catalog = try #require(try JSONSerialization.jsonObject(with: catalogData) as? [String: Any])
        let strings = try #require(catalog["strings"] as? [String: [String: Any]])

        return ExtensionResources(name: name, infoPlist: plist, catalogKeys: strings)
    }

    /// All labels the extension shows in the Finder context menu or as a badge description.
    private static func labels(in infoPlist: [String: Any]) -> [String] {
        guard let nsExtension = infoPlist["NSExtension"] as? [String: Any] else { return [] }

        let actions = (nsExtension["NSExtensionFileProviderActions"] as? [[String: Any]]) ?? []
        let actionNames = actions.compactMap { $0["NSExtensionFileProviderActionName"] as? String }

        let decorations = (nsExtension["NSFileProviderDecorations"] as? [[String: Any]]) ?? []
        let decorationLabels = decorations.compactMap { decoration -> String? in
            let label = decoration["Label"] as? [String: Any]
            return label?["NSStringFormat"] as? String
        }

        return actionNames + decorationLabels
    }

    /// Whether `label` is capitalized in title style, as Apple's human interface guidelines require for menu labels.
    static func isTitleStyle(_ label: String) -> Bool {
        let words = label.split(separator: " ")
        guard !words.isEmpty else { return false }

        return words.enumerated().allSatisfy { index, word in
            guard let first = word.first else { return false }
            if index > 0, lowercaseWords.contains(word.lowercased()) {
                return word == word.lowercased()
            }
            return first.isUppercase
        }
    }

    @Test func titleStyleCheckAcceptsAndRejectsLabels() {
        #expect(Self.isTitleStyle("Always Keep Downloaded"))
        #expect(Self.isTitleStyle("Open in Browser"))
        #expect(Self.isTitleStyle("Free Up Space"))
        #expect(Self.isTitleStyle("Lock File"))

        #expect(!Self.isTitleStyle("Always keep downloaded"))
        #expect(!Self.isTitleStyle("Open In Browser"))
        #expect(!Self.isTitleStyle("open in Browser"))
        #expect(!Self.isTitleStyle("Free up space"))
        #expect(!Self.isTitleStyle(""))
    }

    @Test(arguments: extensionNames)
    func extensionDeclaresLabels(extensionName: String) throws {
        let resources = try Self.loadResources(of: extensionName)

        #expect(!Self.labels(in: resources.infoPlist).isEmpty)
    }

    @Test(arguments: extensionNames)
    func labelsAreTitleStyle(extensionName: String) throws {
        let resources = try Self.loadResources(of: extensionName)

        for label in Self.labels(in: resources.infoPlist) {
            #expect(Self.isTitleStyle(label), "\(resources.name): \"\(label)\" is not in title-style capitalization.")
        }
    }

    @Test(arguments: extensionNames)
    func labelsAreKeysOfTheStringCatalog(extensionName: String) throws {
        let resources = try Self.loadResources(of: extensionName)

        for label in Self.labels(in: resources.infoPlist) {
            #expect(
                resources.catalogKeys[label] != nil,
                "\(resources.name): \"\(label)\" is not a key in Localizable.xcstrings, translations would not be found."
            )
        }
    }

    @Test(arguments: extensionNames)
    func englishValuesMatchTheLabels(extensionName: String) throws {
        let resources = try Self.loadResources(of: extensionName)

        for label in Self.labels(in: resources.infoPlist) {
            let localizations = resources.catalogKeys[label]?["localizations"] as? [String: [String: Any]]

            for language in ["en", "en_GB"] {
                let unit = localizations?[language]?["stringUnit"] as? [String: Any]
                #expect(
                    unit?["value"] as? String == label,
                    "\(resources.name): \(language) value of \"\(label)\" differs from the label."
                )
            }
        }
    }

    @Test(arguments: extensionNames)
    func catalogHasNoKeysDifferingOnlyInCase(extensionName: String) throws {
        let resources = try Self.loadResources(of: extensionName)

        let keysByLowercasedKey = Dictionary(grouping: resources.catalogKeys.keys) { $0.lowercased() }
        let collisions = keysByLowercasedKey.values.filter { $0.count > 1 }

        #expect(collisions.isEmpty, "\(resources.name): keys differing only in case: \(collisions)")
    }
}
