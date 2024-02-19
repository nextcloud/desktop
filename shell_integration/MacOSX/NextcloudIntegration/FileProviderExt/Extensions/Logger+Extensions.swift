/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import OSLog

extension Logger {
    private static var subsystem = Bundle.main.bundleIdentifier!

    static let desktopClientConnection = Logger(
        subsystem: subsystem, category: "desktopclientconnection")
    static let enumeration = Logger(subsystem: subsystem, category: "enumeration")
    static let fileProviderExtension = Logger(
        subsystem: subsystem, category: "fileproviderextension")
    static let fileTransfer = Logger(subsystem: subsystem, category: "filetransfer")
    static let localFileOps = Logger(subsystem: subsystem, category: "localfileoperations")
    static let ncFilesDatabase = Logger(subsystem: subsystem, category: "nextcloudfilesdatabase")
    static let materialisedFileHandling = Logger(
        subsystem: subsystem, category: "materialisedfilehandling"
    )
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

