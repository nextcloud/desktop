//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import NextcloudFileProviderKit
import os

public actor FileProviderLogMock: FileProviderLogging {
    let logger: Logger

    public init() {
        logger = Logger(subsystem: Bundle.main.bundleIdentifier!, category: "FileProviderLogMock")
    }

    public func write(category: String, level: OSLogType, message: String, details: [FileProviderLogDetailKey : Any?]) {
        logger.debug("\(message, privacy: .public)")
    }
}
