//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for what the working-set scan remembers per directory and asks while it walks.
    ///
    @Suite("Scanned remote state")
    struct ScannedRemoteStateTests {
        let manager = DatabaseTestSuites.makeManager()

        private func makeDirectory(ocId: String, fileName: String, serverUrl: String? = nil, visited: Bool = false) -> SendableItemMetadata {
            var directory = DatabaseTestSuites.makeFile(ocId: ocId, fileName: fileName, serverUrl: serverUrl)
            directory.directory = true
            directory.visitedDirectory = visited
            directory.etag = "\(ocId)-etag"
            return directory
        }

        @Test func theStateIsBuiltFromEtagAndPermissions() {
            var metadata = DatabaseTestSuites.makeFile(ocId: "d", fileName: "d")
            metadata.etag = "e1"
            metadata.permissions = "RGDNVW"

            #expect(FilesDatabaseManager.scanRemoteState(of: metadata) == "e1|RGDNVW")
        }

        @Test func recordedStatesAreReadBackForDirectoriesOnly() throws {
            try manager.insertForTesting(makeDirectory(ocId: "a", fileName: "a"))
            try manager.insertForTesting(makeDirectory(ocId: "b", fileName: "b"))
            try manager.insertForTesting(DatabaseTestSuites.makeFile(ocId: "f", fileName: "f.txt"))
            #expect(manager.scannedRemoteStates() == [:])

            manager.recordScannedRemoteStates(["a": "a-state", "f": "f-state", "unknown": "x"], forgetting: [])

            #expect(manager.scannedRemoteStates() == ["a": "a-state"], "Files and unknown identifiers are not remembered.")
            #expect(manager.scannedRemoteStateForTesting(ocId: "b") == nil)
        }

        @Test func theStatesOfGivenDirectoriesAreReadBack() throws {
            let count = FilesDatabaseManager.inClauseChunkSize + 2
            var states: [String: String] = [:]
            for index in 0 ..< count {
                try manager.insertForTesting(makeDirectory(ocId: "d-\(index)", fileName: "d-\(index)"))
                states["d-\(index)"] = "s-\(index)"
            }
            try manager.insertForTesting(makeDirectory(ocId: "never", fileName: "never"))
            manager.recordScannedRemoteStates(states, forgetting: [])

            let asked = Array(states.keys) + ["never", "unknown"]

            #expect(manager.scannedRemoteStates(ocIds: asked) == states, "Directories never reconciled and unknown identifiers are left out.")
            #expect(manager.scannedRemoteStates(ocIds: []) == [:])
        }

        @Test func forgettingWinsOverRecordingInTheSameWrite() throws {
            try manager.insertForTesting(makeDirectory(ocId: "a", fileName: "a"))
            try manager.insertForTesting(makeDirectory(ocId: "b", fileName: "b"))
            manager.recordScannedRemoteStates(["a": "a-state", "b": "b-state"], forgetting: [])

            manager.recordScannedRemoteStates(["a": "a-newer", "b": "b-newer"], forgetting: ["a"])

            #expect(manager.scannedRemoteStates() == ["b": "b-newer"], "A directory on a failed chain is forgotten even if a sibling walk reconciled it.")
        }

        @Test func theStateSurvivesEveryRowRewrite() {
            let directory = makeDirectory(ocId: "a", fileName: "a")
            manager.addItemMetadata(directory)
            manager.recordScannedRemoteStates(["a": "a-state"], forgetting: [])

            var changed = directory
            changed.etag = "changed"
            manager.addItemMetadata(changed)
            manager.addItemMetadataPreservingLocalState(changed)
            manager.renameItemMetadata(ocId: "a", newServerUrl: changed.serverUrl, newFileName: "renamed")

            #expect(manager.scannedRemoteStateForTesting(ocId: "a") == "a-state", "The record's upsert does not touch the column.")
        }

        @Test func forgettingMoreThanOneChunkOfIdentifiers() throws {
            let count = FilesDatabaseManager.inClauseChunkSize + 3
            var states: [String: String] = [:]
            for index in 0 ..< count {
                try manager.insertForTesting(makeDirectory(ocId: "d-\(index)", fileName: "d-\(index)"))
                states["d-\(index)"] = "s"
            }
            manager.recordScannedRemoteStates(states, forgetting: [])
            #expect(manager.scannedRemoteStates()?.count == count)

            manager.recordScannedRemoteStates([:], forgetting: Set(states.keys))

            #expect(manager.scannedRemoteStates() == [:])
        }

        @Test func materialisedItemsBelowADirectoryAreFound() throws {
            let root = DatabaseTestSuites.account.davFilesUrl
            try manager.insertForTesting(makeDirectory(ocId: "a", fileName: "a"))
            try manager.insertForTesting(makeDirectory(ocId: "b", fileName: "b", serverUrl: root + "/a"))
            var deep = DatabaseTestSuites.makeFile(ocId: "deep", fileName: "deep.txt", serverUrl: root + "/a/b")
            deep.downloaded = true
            try manager.insertForTesting(deep)
            var gone = DatabaseTestSuites.makeFile(ocId: "gone", fileName: "gone.txt", serverUrl: root + "/other")
            gone.downloaded = true
            gone.deleted = true
            try manager.insertForTesting(gone)
            try manager.insertForTesting(makeDirectory(ocId: "other", fileName: "other"))

            #expect(manager.hasMaterialisedItem(below: root + "/a") == true)
            #expect(manager.hasMaterialisedItem(below: root + "/a/b") == true)
            #expect(manager.hasMaterialisedItem(below: root + "/other") == false, "A deleted row does not count.")
            #expect(manager.hasMaterialisedItem(below: root + "/a/bb") == false, "A sibling with a common prefix is not below.")
        }

        @Test func externalStoragesBelowADirectoryAreFound() throws {
            let root = DatabaseTestSuites.account.davFilesUrl
            try manager.insertForTesting(makeDirectory(ocId: "a", fileName: "a"))
            var mount = makeDirectory(ocId: "mount", fileName: "mount", serverUrl: root + "/a")
            mount.mountType = FilesDatabaseManager.externalMountType
            try manager.insertForTesting(mount)
            var gone = makeDirectory(ocId: "gone", fileName: "gone", serverUrl: root + "/b")
            gone.mountType = FilesDatabaseManager.externalMountType
            gone.deleted = true
            try manager.insertForTesting(gone)
            try manager.insertForTesting(makeDirectory(ocId: "b", fileName: "b"))

            #expect(manager.hasExternalStorage(below: root + "/a") == true)
            #expect(manager.hasExternalStorage(below: root) == true)
            #expect(manager.hasExternalStorage(below: root + "/b") == false, "A deleted row does not count.")
            #expect(manager.hasExternalStorage(below: root + "/a/mount") == false, "The mount's own row is not below itself.")
        }

        @Test func aLiveOccupantOfAnAddressIsFound() throws {
            let root = DatabaseTestSuites.account.davFilesUrl
            var old = makeDirectory(ocId: "d-old", fileName: "d")
            old.deleted = true
            try manager.insertForTesting(old)
            #expect(manager.liveItemOccupies(serverUrl: root, fileName: "d", otherThan: "d-old") == false)

            try manager.insertForTesting(makeDirectory(ocId: "d-new", fileName: "d"))

            #expect(manager.liveItemOccupies(serverUrl: root, fileName: "d", otherThan: "d-old") == true)
            #expect(manager.liveItemOccupies(serverUrl: root, fileName: "d", otherThan: "d-new") == false, "The row itself does not count.")
        }

        @Test func failedReadsAnswerNilAndFailedWritesAreLogged() throws {
            try manager.insertForTesting(makeDirectory(ocId: "a", fileName: "a"))
            manager.recordScannedRemoteStates(["a": "a-state"], forgetting: [])
            try manager.breakTableForTesting(ItemMetadataRecord.databaseTableName)

            #expect(manager.scannedRemoteStates() == nil)
            #expect(manager.scannedRemoteStates(ocIds: ["a"]) == nil)
            #expect(manager.hasMaterialisedItem(below: DatabaseTestSuites.account.davFilesUrl + "/a") == nil)
            #expect(manager.hasExternalStorage(below: DatabaseTestSuites.account.davFilesUrl + "/a") == nil)
            #expect(manager.liveItemOccupies(serverUrl: DatabaseTestSuites.account.davFilesUrl, fileName: "a", otherThan: "x") == nil)
            manager.recordScannedRemoteStates(["a": "b"], forgetting: ["a"])
            manager.recordScannedRemoteStates([:], forgetting: [])
        }
    }
}
