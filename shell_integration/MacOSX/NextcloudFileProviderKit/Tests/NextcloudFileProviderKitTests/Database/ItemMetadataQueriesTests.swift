//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for the location predicates: path boundaries, normalization, case and pattern characters.
    ///
    @Suite("Item metadata queries")
    struct ItemMetadataQueriesTests {
        let manager = DatabaseTestSuites.makeManager()
        let root = DatabaseTestSuites.account.davFilesUrl

        private func directory(ocId: String, fileName: String, parent: String) -> SendableItemMetadata {
            var metadata = DatabaseTestSuites.makeFile(ocId: ocId, fileName: fileName, serverUrl: parent)
            metadata.directory = true
            return metadata
        }

        @Test(arguments: [
            ("photos", "photos-backup"), ("docs", "docs-archive"), ("work", "work-old"), ("alpha", "alphabet"),
            ("project", "project-v2"), ("folder", "folderX"), ("folder", "folder.old"), ("a_b", "aXb"), ("50%", "50X")
        ])
        func descendantLookupStopsAtThePathBoundary(parentName: String, siblingName: String) throws {
            let parent = directory(ocId: "parent", fileName: parentName, parent: root)
            let sibling = directory(ocId: "sibling", fileName: siblingName, parent: root)
            let child = DatabaseTestSuites.makeFile(ocId: "child", fileName: "child.txt", serverUrl: root + "/" + parentName)
            let siblingChild = DatabaseTestSuites.makeFile(ocId: "sibling-child", fileName: "child.txt", serverUrl: root + "/" + siblingName)
            let grandchild = DatabaseTestSuites.makeFile(ocId: "grandchild", fileName: "deep.txt", serverUrl: root + "/" + parentName + "/sub")
            for row in [parent, sibling, child, siblingChild, grandchild] {
                try manager.insertForTesting(row)
            }

            #expect(Set(manager.childItems(directoryMetadata: parent).map(\.ocId)) == ["child", "grandchild"])
            #expect(manager.immediateChildItems(directoryMetadata: parent).map(\.ocId) == ["child"])
            #expect(manager.childItemCount(directoryMetadata: parent) == 1)
        }

        @Test func descendantLookupNormalizesTheQueryToNFC() throws {
            let decomposedName = "Pre\u{0302}t"
            let parent = directory(ocId: "parent", fileName: decomposedName, parent: root)
            let child = DatabaseTestSuites.makeFile(ocId: "child", fileName: "child.txt", serverUrl: root + "/" + decomposedName)
            try manager.insertForTesting(parent)
            try manager.insertForTesting(child)

            var precomposedParent = parent
            precomposedParent.fileName = decomposedName.precomposedStringWithCanonicalMapping

            #expect(manager.childItems(directoryMetadata: precomposedParent).map(\.ocId) == ["child"])
            #expect(manager.itemMetadata(account: parent.account, locatedAtRemoteUrl: root + "/" + decomposedName.precomposedStringWithCanonicalMapping)?.ocId == "parent")
        }

        @Test func locationLookupIsCaseSensitive() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "upper", fileName: "File.txt"))

            #expect(manager.itemMetadata(account: "", locatedAtRemoteUrl: root + "/File.txt")?.ocId == "upper")
            #expect(manager.itemMetadata(account: "", locatedAtRemoteUrl: root + "/file.txt") == nil)
        }

        @Test func locationLookupPrefersTheLiveRowOverATombstone() throws {
            var tombstone = DatabaseTestSuites.makeFile(ocId: "old", fileName: "same.txt")
            tombstone.deleted = true
            try manager.insertForTesting(tombstone)
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "zzz-live", fileName: "same.txt"))

            #expect(manager.itemMetadata(account: "", locatedAtRemoteUrl: root + "/same.txt")?.ocId == "zzz-live")
        }

        @Test func suffixLookupIsCaseSensitiveAndTakesPatternCharactersLiterally() throws {
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "lower", fileName: "report.pdf"))
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "upper", fileName: "REPORT.PDF"))
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "bracket", fileName: "draft[1].txt"))
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "percent", fileName: "100%.txt"))
            var folder = DatabaseTestSuites.makeFile(ocId: "folder", fileName: "folder.pdf")
            folder.directory = true
            try manager.insertForTesting(folder)

            #expect(manager.itemsMetadataByFileNameSuffix(suffix: ".pdf").map(\.ocId) == ["lower"])
            #expect(manager.itemsMetadataByFileNameSuffix(suffix: "[1].txt").map(\.ocId) == ["bracket"])
            #expect(manager.itemsMetadataByFileNameSuffix(suffix: "%.txt").map(\.ocId) == ["percent"])
            #expect(manager.itemsMetadataByFileNameSuffix(suffix: "_.txt").isEmpty)
            #expect(Set(manager.itemsMetadataByFileNameSuffix(suffix: "").map(\.ocId)) == ["lower", "upper", "bracket", "percent"])
        }

        @Test func fileIdMembershipHandlesMoreThanOneChunkOfIdentifiers() throws {
            try manager.insertForTesting({
                var metadata = DatabaseTestSuites.makeFile(ocId: "known", fileName: "known.txt")
                metadata.fileId = "999999"
                return metadata
            }())

            var ids = Set((0 ..< 1200).map(String.init))
            #expect(manager.containsAnyItemMetadata(fileIds: ids) == false)

            ids.insert("999999")
            #expect(manager.containsAnyItemMetadata(fileIds: ids))
            #expect(manager.containsAnyItemMetadata(fileIds: []) == false)
        }
    }
}
