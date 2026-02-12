//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import os

///
/// Requirements for a log implementation which enables a mock object for tests.
///
public protocol FileProviderLogging: Actor {
    ///
    /// Write a message to the unified logging system and current log file.
    ///
    /// Usually, you do not need or want to use this but the methods provided by ``FileProviderLogger`` instead.
    ///
    /// - Parameters:
    ///     - category: The unified logging category to use. Usually, this is the logging type.
    ///     - level: The severity of the message.
    ///     - message: A human-readable message, preferably generic and without interpolations. The `details` argument is for arguments.
    ///     - details: Structured and contextual details about a message.
    ///     - file: Implementations should have `#filePath` as the default value for this.
    ///     - function: Implementations should have `#function` as the default value for this.
    ///     - line: Implementations should have `#line` as the default value for this.
    ///
    func write(category: String, level: OSLogType, message: String, details: [FileProviderLogDetailKey: (any Sendable)?], file: StaticString, function: StaticString, line: UInt)
}
