//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import NextcloudKit
import Testing
import TestInterface

@Suite("File provider error of a server response")
struct NKErrorFileProviderErrorTests {
    @Test("A request timeout from the server or a proxy is a temporary outage", arguments: [408, 502, 504, 524])
    func gatewayTimeoutIsServerUnreachable(statusCode: Int) {
        let error = NKError(statusCode: statusCode, fallbackDescription: "Timeout")

        #expect(error.fileProviderError == NSFileProviderError(.serverUnreachable))
    }

    @Test("Another server error cannot be synchronized", arguments: [400, 501, 505])
    func otherServerErrorCannotBeSynchronized(statusCode: Int) {
        let error = NKError(statusCode: statusCode, fallbackDescription: "Server error")

        #expect(error.fileProviderError == NSFileProviderError(.cannotSynchronize))
    }

    @Test("An upload refused for lack of local disk space reports the full disk")
    func localStorageFullIsOutOfSpace() async {
        let account = DatabaseTestSuites.account
        let error = await NKError.errorChunkNoEnoughMemory.fileProviderError(
            handlingCollisionAgainstItemInRemotePath: account.davFilesUrl + "/file",
            dbManager: DatabaseTestSuites.makeManager(),
            remoteInterface: MockRemoteInterface(account: account),
            log: FileProviderLogMock()
        )

        #expect((error as? CocoaError)?.code == .fileWriteOutOfSpace)
    }
}
