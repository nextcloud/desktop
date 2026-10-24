//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

/// Selects the directories used for File Provider temporary data and sync metadata.
enum FileProviderDomainStorage {
    private static let stateDirectoryRetryDelaysNanoseconds: [UInt64] = [100_000_000, 250_000_000]

    static func temporaryDirectory(
        isExternalDomain: Bool,
        domainTemporaryDirectory: (() throws -> URL)?,
        fallbackDirectory: () -> URL
    ) throws -> URL {
        guard let domainTemporaryDirectory else {
            return fallbackDirectory()
        }

        do {
            return try domainTemporaryDirectory()
        } catch {
            guard isExternalDomain else {
                return fallbackDirectory()
            }

            throw error
        }
    }

    static func databaseDirectory(
        volumeUUID: UUID?,
        stateDirectory: () throws -> URL,
        sleep: (UInt64) async throws -> Void = { try await Task.sleep(nanoseconds: $0) }
    ) async throws -> URL? {
        guard volumeUUID != nil else {
            return nil
        }

        for (index, delay) in stateDirectoryRetryDelaysNanoseconds.enumerated() {
            do {
                return try stateDirectory()
            } catch {
                try await sleep(delay)

                if index == stateDirectoryRetryDelaysNanoseconds.indices.last {
                    return try stateDirectory()
                }
            }
        }

        return try stateDirectory()
    }
}
