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

    public func write(category _: String, level _: OSLogType, message: String, details _: [FileProviderLogDetailKey: Any?]) {
        logger.debug("\(message, privacy: .public)")
    }
}
