//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for ``FilesDatabaseManager/setMaterializationStates(_:durability:)``, which must not write back anything but the flags it is given.
    ///
    @Suite("Materialization state")
    struct FilesDatabaseManagerMaterializationStateTests {
        let manager = DatabaseTestSuites.makeManager()

        @Test func writesOnlyTheGivenFlags() throws {
            var file = DatabaseTestSuites.makeFile(ocId: "file", fileName: "report.pdf")
            file.status = Status.downloading.rawValue
            file.etag = "etag-during-download"
            var directory = DatabaseTestSuites.makeFile(ocId: "directory", fileName: "docs")
            directory.directory = true
            directory.downloaded = true
            try manager.insertForTesting(file)
            try manager.insertForTesting(directory)

            manager.setMaterializationStates([
                (ocId: "file", downloaded: true, visitedDirectory: nil),
                (ocId: "directory", downloaded: nil, visitedDirectory: true)
            ])

            let storedFile = try #require(manager.itemMetadata(ocId: "file"))
            #expect(storedFile.downloaded)
            #expect(!storedFile.visitedDirectory)
            #expect(storedFile.status == Status.downloading.rawValue, "A status written since the pass read the row must survive.")
            #expect(storedFile.etag == "etag-during-download")

            let storedDirectory = try #require(manager.itemMetadata(ocId: "directory"))
            #expect(storedDirectory.visitedDirectory)
            #expect(storedDirectory.downloaded, "A flag the pass did not change must not be written back.")
        }

        @Test func leavesAMissingRowAbsent() {
            manager.setMaterializationStates([(ocId: "deleted-meanwhile", downloaded: false, visitedDirectory: false)])

            #expect(manager.itemMetadata(ocId: "deleted-meanwhile") == nil)
        }
    }
}
