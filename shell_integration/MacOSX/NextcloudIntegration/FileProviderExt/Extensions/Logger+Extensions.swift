/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import OSLog

extension Logger {
    private static var subsystem = Bundle.main.bundleIdentifier!

    static let desktopClientConnection = Logger(
        subsystem: subsystem, category: "desktopclientconnection")
    static let fileProviderDomainDefaults = Logger(subsystem: subsystem, category: "fileProviderDomainDefaults")
    static let fpUiExtensionService = Logger(subsystem: subsystem, category: "fpUiExtensionService")
    static let fileProviderExtension = Logger(
        subsystem: subsystem, category: "fileproviderextension")
    static let keychain = Logger(subsystem: subsystem, category: "keychain")
    static let shares = Logger(subsystem: subsystem, category: "shares")
    static let logger = Logger(subsystem: subsystem, category: "logger")

    @available(macOSApplicationExtension 12.0, *)
    static func logEntries(interval: TimeInterval = -3600) -> (Array<String>?, Error?) {
        do {
            let logStore = try OSLogStore(scope: .currentProcessIdentifier)
            let timeDate = Date().addingTimeInterval(interval)
            let logPosition = logStore.position(date: timeDate)
            let entries = try logStore.getEntries(at: logPosition)

            return (entries
                .compactMap { $0 as? OSLogEntryLog }
                .filter { $0.subsystem == Logger.subsystem }
                .map { $0.composedMessage }, nil)

        } catch let error {
            Logger.logger.error("Could not acquire os log store: \(error)");
            return (nil, error)
        }
    }
}

