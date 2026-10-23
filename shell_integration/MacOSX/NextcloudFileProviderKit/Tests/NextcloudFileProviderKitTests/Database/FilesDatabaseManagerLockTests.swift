//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import NextcloudKit
import Testing

extension DatabaseTestSuites {
    ///
    /// Coverage for the lock state and raw-location lookups of ``FilesDatabaseManager``.
    ///
    @Suite("Locks")
    struct FilesDatabaseManagerLockTests {
        let manager = DatabaseTestSuites.makeManager()
        let parentUrl = DatabaseTestSuites.account.davFilesUrl + "/Documents"

        private func makeLock(token: String = "token", etag: String? = nil) -> NKLock {
            NKLock(
                owner: "alice",
                ownerEditor: "text",
                ownerType: .app,
                ownerDisplayName: "Alice",
                time: Date(timeIntervalSince1970: 1_700_000_000),
                timeOut: Date(timeIntervalSince1970: 1_700_003_600),
                token: token,
                etag: etag
            )
        }

        private func seed(ocId: String = "doc", fileName: String = "Report.docx", etag: String = "etag-1", deleted: Bool = false) -> SendableItemMetadata {
            var metadata = DatabaseTestSuites.makeFile(ocId: ocId, fileName: fileName, serverUrl: parentUrl)
            metadata.etag = etag
            metadata.deleted = deleted
            metadata.syncTime = Date(timeIntervalSince1970: 0)
            manager.addItemMetadata(metadata)
            return metadata
        }

        @Test func applyingALockStoresEveryLockFieldAndStampsTheSyncTime() throws {
            _ = seed()

            #expect(try manager.applyLock(makeLock(), rawServerUrl: parentUrl, rawFileName: "Report.docx"))

            let stored = try #require(manager.itemMetadata(ocId: "doc"))
            #expect(stored.lock)
            #expect(stored.lockOwner == "alice")
            #expect(stored.lockOwnerDisplayName == "Alice")
            #expect(stored.lockOwnerEditor == "text")
            #expect(stored.lockOwnerType == NKLockType.app.rawValue)
            #expect(stored.lockTime == Date(timeIntervalSince1970: 1_700_000_000))
            #expect(stored.lockTimeOut == Date(timeIntervalSince1970: 1_700_003_600))
            #expect(stored.lockToken == "token")
            #expect(stored.syncTime > Date(timeIntervalSince1970: 0))
        }

        @Test func applyingALockWithAnEtagKeepsTheKnownContentVersion() throws {
            _ = seed(etag: "etag-1")

            #expect(try manager.applyLock(makeLock(etag: "etag-2"), rawServerUrl: parentUrl, rawFileName: "Report.docx"))
            var stored = try #require(manager.itemMetadata(ocId: "doc"))
            #expect(stored.etag == "etag-2")
            #expect(stored.fileProviderContentVersion == "etag-1")

            #expect(try manager.applyLock(makeLock(etag: "etag-3"), rawServerUrl: parentUrl, rawFileName: "Report.docx"))
            stored = try #require(manager.itemMetadata(ocId: "doc"))
            #expect(stored.etag == "etag-3")
            #expect(stored.fileProviderContentVersion == "etag-1", "The content version File Provider already knows survives further lock responses.")
        }

        @Test func applyingALockWithoutAnEtagLeavesTheEtagAlone() throws {
            _ = seed(etag: "etag-1")

            #expect(try manager.applyLock(makeLock(), rawServerUrl: parentUrl, rawFileName: "Report.docx"))

            let stored = try #require(manager.itemMetadata(ocId: "doc"))
            #expect(stored.etag == "etag-1")
            #expect(stored.fileProviderContentVersion == nil)
        }

        @Test func applyingALockToAnUnknownLocationReportsFalse() throws {
            _ = seed()

            #expect(try manager.applyLock(makeLock(), rawServerUrl: parentUrl, rawFileName: "Other.docx") == false)
            #expect(try manager.applyLock(makeLock(), rawServerUrl: parentUrl + "/Sub", rawFileName: "Report.docx") == false)
            #expect(manager.itemMetadata(ocId: "doc")?.lock == false)
        }

        @Test func clearingALockRemovesTheLockFieldsAndLeavesTheSyncTimeUntouched() throws {
            _ = seed()
            #expect(try manager.applyLock(makeLock(), rawServerUrl: parentUrl, rawFileName: "Report.docx"))
            let lockedSyncTime = try #require(manager.itemMetadata(ocId: "doc")).syncTime

            #expect(try manager.clearLock(rawServerUrl: parentUrl, rawFileName: "Report.docx"))

            let stored = try #require(manager.itemMetadata(ocId: "doc"))
            #expect(stored.lock == false)
            #expect(stored.lockOwner == nil)
            #expect(stored.lockOwnerDisplayName == nil)
            #expect(stored.lockOwnerEditor == nil)
            #expect(stored.lockOwnerType == nil)
            #expect(stored.lockTime == nil)
            #expect(stored.lockTimeOut == nil)
            #expect(stored.lockToken == nil)
            #expect(stored.syncTime == lockedSyncTime)
        }

        @Test func clearingALockAtAnUnknownLocationReportsFalse() throws {
            #expect(try manager.clearLock(rawServerUrl: parentUrl, rawFileName: "Report.docx") == false)
        }

        @Test func rawLocationLookupComparesTheStoredBytesWithoutNormalization() {
            let decomposed = "Re\u{0301}sume\u{0301}.docx"
            let precomposed = decomposed.precomposedStringWithCanonicalMapping
            _ = seed(ocId: "nfd", fileName: decomposed)

            #expect(manager.itemMetadata(rawServerUrl: parentUrl, rawFileName: decomposed)?.ocId == "nfd")
            #expect(manager.itemMetadata(rawServerUrl: parentUrl, rawFileName: precomposed) == nil)
            #expect(manager.itemMetadata(rawServerUrl: parentUrl, rawFileName: "report.docx") == nil)
        }

        @Test func rawLocationLookupReturnsDeletedItems() {
            _ = seed(ocId: "gone", fileName: "Old.docx", deleted: true)

            #expect(manager.itemMetadata(rawServerUrl: parentUrl, rawFileName: "Old.docx")?.deleted == true)
        }
    }
}
