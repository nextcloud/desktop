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
            isExternalDomain: false,
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
            isExternalDomain: true,
            domainTemporaryDirectory: { expected },
            fallbackDirectory: {
                fallbackRequested = true
                return URL(fileURLWithPath: "/fallback-temp")
            }
        )

        #expect(directory == expected)
        #expect(!fallbackRequested)
    }

    @Test func internalDomainFallsBackWhenTemporaryDirectoryLookupFails() throws {
        let expected = URL(fileURLWithPath: "/fallback-temp")

        let directory = try FileProviderDomainStorage.temporaryDirectory(
            isExternalDomain: false,
            domainTemporaryDirectory: { throw CocoaError(.fileNoSuchFile) },
            fallbackDirectory: { expected }
        )

        #expect(directory == expected)
    }

    @Test func externalDomainPropagatesTemporaryDirectoryError() {
        #expect(throws: CocoaError.self) {
            try FileProviderDomainStorage.temporaryDirectory(
                isExternalDomain: true,
                domainTemporaryDirectory: { throw CocoaError(.fileNoSuchFile) },
                fallbackDirectory: {
                    Issue.record("External domains must not fall back to a different volume.")
                    return URL(fileURLWithPath: "/fallback-temp")
                }
            )
        }
    }

    @Test func internalDomainUsesDefaultDatabaseDirectory() async throws {
        var stateDirectoryRequested = false

        let directory = try await FileProviderDomainStorage.databaseDirectory(
            volumeUUID: nil,
            stateDirectory: {
                stateDirectoryRequested = true
                return URL(fileURLWithPath: "/external-state")
            },
            sleep: { _ in }
        )

        #expect(directory == nil)
        #expect(!stateDirectoryRequested)
    }

    @Test func externalDomainUsesStateDirectory() async throws {
        let expected = URL(fileURLWithPath: "/external-state")

        let directory = try await FileProviderDomainStorage.databaseDirectory(
            volumeUUID: UUID(),
            stateDirectory: { expected },
            sleep: { _ in }
        )

        #expect(directory == expected)
    }

    @Test func externalDomainRetriesStateDirectoryLookup() async throws {
        let expected = URL(fileURLWithPath: "/external-state")
        var attempts = 0
        var sleeps: [UInt64] = []

        let directory = try await FileProviderDomainStorage.databaseDirectory(
            volumeUUID: UUID(),
            stateDirectory: {
                attempts += 1
                if attempts < 3 {
                    throw CocoaError(.fileNoSuchFile)
                }
                return expected
            },
            sleep: { sleeps.append($0) }
        )

        #expect(directory == expected)
        #expect(attempts == 3)
        #expect(sleeps.count == 2)
    }

    @Test func externalDomainPropagatesStateDirectoryErrorAfterRetries() async {
        var attempts = 0

        await #expect(throws: CocoaError.self) {
            _ = try await FileProviderDomainStorage.databaseDirectory(
                volumeUUID: UUID(),
                stateDirectory: {
                    attempts += 1
                    throw CocoaError(.fileNoSuchFile)
                },
                sleep: { _ in }
            )
        }

        #expect(attempts == 3)
    }
}
