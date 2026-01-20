//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

///
/// A class which encapsulates the low-level management of file provider domains.
///
final class FileProviderDomainManager {
    ///
    /// Add a new file provider domain for the given account.
    ///
    func add(for account: Account) async throws -> NSFileProviderDomain {
        let uuid = UUID()
        let identifier = NSFileProviderDomainIdentifier(uuid.uuidString)
        let domain = NSFileProviderDomain(identifier: identifier, displayName: account.displayName)
        try await NSFileProviderManager.add(domain)

        return domain
    }

    ///
    /// Remove the file provider domain of the given account.
    ///
    func remove(for account: Account) async throws {
        guard let domain = account.domain else {
            throw FileProviderAppError.fileProviderDomainNotFound
        }

        try await NSFileProviderManager.remove(domain, mode: .removeAll)
    }

    ///
    /// Clear all domains managed by this app.
    ///
    func removeAll() async throws {
        try await NSFileProviderManager.removeAllDomains()
    }
}
