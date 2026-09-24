//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import NextcloudFileProviderKit
import os

///
/// One `.error` or `.fault` message that production code emitted during a test.
///
public struct LoggedProblem: Sendable, CustomStringConvertible {
    public let level: String
    public let category: String
    public let message: String

    public var description: String {
        "[\(level)] \(category): \(message)"
    }
}

///
/// Collects the `.error` and `.fault` messages written through ``FileProviderLogMock`` so that a test
/// harness can fail on them.
///
/// Production code logs and carries on wherever it cannot throw — `ChangeDeliveryBuffer` abandoning a
/// batch it cannot acknowledge, for one — so a test that then asserts the wrong state gives no hint as
/// to why, and a test that asserts nothing passes with the complaint unread. The store is static
/// because tests build their mocks ad hoc, often several per test and inside helpers, so there is no
/// one instance a harness could inspect.
///
public enum FileProviderLogProblemRecorder {
    private static let lock = NSLock()
    private nonisolated(unsafe) static var storage: [LoggedProblem] = []

    public static func record(_ problem: LoggedProblem) {
        lock.lock()
        defer { lock.unlock() }
        storage.append(problem)
    }

    /// Everything recorded since the last ``reset()``.
    public static var problems: [LoggedProblem] {
        lock.lock()
        defer { lock.unlock() }
        return storage
    }

    public static func reset() {
        lock.lock()
        defer { lock.unlock() }
        storage.removeAll()
    }
}

public actor FileProviderLogMock: FileProviderLogging {
    public let debugLoggingEnabled: Bool = true
    public let performanceLoggingEnabled: Bool = true

    let logger: Logger

    public init() {
        logger = Logger(subsystem: Bundle.main.bundleIdentifier ?? "com.nextcloud.NextcloudFileProviderKit.tests", category: "FileProviderLogMock")
    }

    public func write(category: String, level: OSLogType, message: String, details _: [FileProviderLogDetailKey: (any Sendable)?], file _: StaticString, function _: StaticString, line _: UInt) {
        logger.debug("\(message, privacy: .public)")

        guard level == .error || level == .fault else {
            return
        }

        FileProviderLogProblemRecorder.record(
            LoggedProblem(level: level == .fault ? "fault" : "error", category: category, message: message)
        )
    }
}
