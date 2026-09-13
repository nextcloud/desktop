//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct FileProviderDomainStorageTests {
    @Test func noDomainUsesFallbackTemporaryDirectory() throws {
        let expected = URL(fileURLWithPath: "/fallback-temp")
        var fallbackRequested = false

        let directory = try FileProviderDomainStorage.temporaryDirectory(
            domainTemporaryDirectory: nil,
            fallbackDirectory: {
                fallbackRequested = true
                return expected
            }
        )

        #expect(directory == expected)
        #expect(fallbackRequested)
    }

    @Test func domainUsesFileProviderTemporaryDirectory() throws {
        let expected = URL(fileURLWithPath: "/domain-temp")
        var fallbackRequested = false

        let directory = try FileProviderDomainStorage.temporaryDirectory(
            domainTemporaryDirectory: { expected },
            fallbackDirectory: {
                fallbackRequested = true
                return URL(fileURLWithPath: "/fallback-temp")
            }
        )

        #expect(directory == expected)
        #expect(!fallbackRequested)
    }

    @Test func domainPropagatesTemporaryDirectoryError() {
        enum TestError: Error {
            case expected
        }

        #expect(throws: TestError.self) {
            try FileProviderDomainStorage.temporaryDirectory(
                domainTemporaryDirectory: { throw TestError.expected },
                fallbackDirectory: {
                    Issue.record("Fallback must not be used when a domain temporary directory lookup fails.")
                    return URL(fileURLWithPath: "/fallback-temp")
                }
            )
        }
    }

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
