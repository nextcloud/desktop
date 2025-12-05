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
    func write(category: String, level: OSLogType, message: String, details: [FileProviderLogDetailKey: (any Sendable)?])
}
