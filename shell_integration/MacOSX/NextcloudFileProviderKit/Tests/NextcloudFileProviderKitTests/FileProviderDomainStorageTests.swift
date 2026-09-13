//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct FileProviderDomainStorageTests {
    @Test func internalDomainUsesDefaultDatabaseDirectory() throws {
        var stateDirectoryRequested = false
        var stateDirectoryAccessRequested = false

        let directory = try FileProviderDomainStorage.databaseDirectory(
            volumeUUID: nil,
            stateDirectory: {
                stateDirectoryRequested = true
                return URL(fileURLWithPath: "/external-state")
            },
            accessStateDirectory: { _ in
                stateDirectoryAccessRequested = true
            }
        )

        #expect(directory == nil)
        #expect(!stateDirectoryRequested)
        #expect(!stateDirectoryAccessRequested)
    }

    @Test func externalDomainUsesAndAccessesStateDirectory() throws {
        let expected = URL(fileURLWithPath: "/external-state")
        var accessedDirectory: URL?

        let directory = try FileProviderDomainStorage.databaseDirectory(
            volumeUUID: UUID(),
            stateDirectory: {
                expected
            },
            accessStateDirectory: { directory in
                accessedDirectory = directory
            }
        )

        #expect(directory == expected)
        #expect(accessedDirectory == expected)
    }

    @Test func externalDomainPropagatesStateDirectoryError() {
        enum TestError: Error {
            case expected
        }

        #expect(throws: TestError.self) {
            try FileProviderDomainStorage.databaseDirectory(
                volumeUUID: UUID(),
                stateDirectory: {
                    throw TestError.expected
                },
                accessStateDirectory: { _ in
                    Issue.record("State-directory access should not run after lookup fails.")
                }
            )
        }
    }

    @Test func externalDomainPropagatesStateDirectoryAccessError() {
        enum TestError: Error {
            case expected
        }

        #expect(throws: TestError.self) {
            try FileProviderDomainStorage.databaseDirectory(
                volumeUUID: UUID(),
                stateDirectory: {
                    URL(fileURLWithPath: "/external-state")
                },
                accessStateDirectory: { _ in
                    throw TestError.expected
                }
            )
        }
    }
}
