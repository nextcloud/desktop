// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import Testing
import UniformTypeIdentifiers

struct RemoveDownloadActionTests {
    private func activationRule() throws -> NSPredicate {
        let package = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let url = package.deletingLastPathComponent().appendingPathComponent("NextcloudIntegration/FileProviderExt/Info.plist")
        let plist = try #require(PropertyListSerialization.propertyList(from: Data(contentsOf: url), format: nil) as? [String: Any])
        let extensionInfo = try #require(plist["NSExtension"] as? [String: Any])
        let actions = try #require(extensionInfo["NSExtensionFileProviderActions"] as? [[String: Any]])
        let action = try #require(actions.first {
            $0["NSExtensionFileProviderActionIdentifier"] as? String == "com.nextcloud.desktopclient.FileProviderExt.EvictAction"
        })
        let rule = try #require(action["NSExtensionFileProviderActionActivationRule"] as? String)
        return NSPredicate(format: rule)
    }

    private func file(downloaded: Bool = true, uploaded: Bool = true, pinned: Bool = false, type: UTType = .pdf) -> [String: Any] {
        [
            "isDownloaded": downloaded,
            "isUploaded": uploaded,
            "contentType": ["identifier": type.identifier],
            "userInfo": ["displayEvict": false, "displayAllowAutoEvicting": pinned]
        ]
    }

    @Test func downloadedFileCanBeEvictedWhenProviderFlagIsStale() throws {
        let predicate = try activationRule()
        #expect(predicate.evaluate(with: ["fileproviderItems": [file()]]))
        #expect(predicate.evaluate(with: ["fileproviderItems": [file(), file()]]))
    }

    @Test func selectionMustContainOnlyEvictableFiles() throws {
        let predicate = try activationRule()
        #expect(!predicate.evaluate(with: ["fileproviderItems": []]))
        for ineligible in [file(downloaded: false), file(uploaded: false), file(pinned: true), file(type: .folder), file(type: .directory), file(type: .bundle)] {
            #expect(!predicate.evaluate(with: ["fileproviderItems": [ineligible]]))
            #expect(!predicate.evaluate(with: ["fileproviderItems": [file(), ineligible]]))
        }
    }
}
