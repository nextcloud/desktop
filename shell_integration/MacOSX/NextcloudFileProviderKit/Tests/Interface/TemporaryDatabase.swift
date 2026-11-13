//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks

///
/// A file-backed ``FilesDatabaseManager`` in a fresh temporary directory, so one test cannot see another's rows.
///
/// Call ``remove()`` when done; the directory and the domain's settings are otherwise left behind.
///
public struct TemporaryDatabase {
    public let manager: FilesDatabaseManager
    public let directory: URL
    private let domainIdentifier: NSFileProviderDomainIdentifier
    private let defaults: UserDefaults

    ///
    /// - Parameters:
    ///   - account: The account the database belongs to.
    ///   - defaults: Where the manager records its domain's settings, so the test host's standard defaults stay untouched.
    ///
    public init(account: Account, defaults: UserDefaults) throws {
        directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("TemporaryDatabase-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        domainIdentifier = NSFileProviderDomainIdentifier("test-\(UUID().uuidString)")
        self.defaults = defaults

        manager = try FilesDatabaseManager(
            account: account,
            databaseDirectory: directory,
            fileProviderDomainIdentifier: domainIdentifier,
            log: FileProviderLogMock(),
            defaults: defaults
        )
    }

    ///
    /// Delete the database directory and the settings ``FileProviderDomainDefaults`` keeps for the domain.
    ///
    public func remove() {
        try? FileManager.default.removeItem(at: directory)
        defaults.removeObject(forKey: domainIdentifier.rawValue)
    }
}
