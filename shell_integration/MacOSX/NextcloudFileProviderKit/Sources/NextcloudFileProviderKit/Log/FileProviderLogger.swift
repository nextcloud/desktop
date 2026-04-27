//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit
import os

///
/// A proxy type to be used by logging types.
///
/// - Automatically augments messages with the once configured category.
/// - Translates the designated methods to the log level argument.
/// - Provides synchronous methods to dispatch log messages to the underlying actor.
///
/// Each instance holds its own `Logger` for forwarding to Apple's unified logging system, tagged with the configured ``category`` so messages are searchable per calling type.
/// The JSON Lines log file is written through the shared ``FileProviderLogging`` actor.
///
/// This must be instantiated per instance of calling types.
/// Statically or lazily defining a single instance of this for all instances of a calling type is not possible due to the dependency on ``FileProviderLog``.
/// The latter needs to be provided as an argument, passed down the call stack from the `NSFileProviderReplicatedExtension` implementation.
///
public struct FileProviderLogger: Sendable {
    ///
    /// The category string to be used as with the unified logging system.
    ///
    let category: String

    ///
    /// The file logging system object.
    ///
    let log: any FileProviderLogging

    ///
    /// The underlying unified logger tagged with ``category``.
    ///
    let logger: Logger

    ///
    /// Create a new logger which is supposed to be used by individual types and their instances.
    ///
    public init(category: String, log: any FileProviderLogging) {
        self.category = category
        self.log = log
        logger = Logger(subsystem: Bundle.main.bundleIdentifier ?? "", category: category)
    }

    ///
    /// Dispatch a task to write a message with the level `OSLogType.debug`.
    ///
    /// Inclusion of debug messages is gated by the shared ``FileProviderLogging/debugLoggingEnabled`` flag; both unified logging and the JSONL file output are skipped when the flag is off.
    ///
    /// - Parameters:
    ///     - message: A human-readable message, preferably generic and without interpolations. The `details` argument is for arguments.
    ///     - details: Structured and contextual details about a message.
    ///     - file: Implementations should have `#filePath` as the default value for this.
    ///     - function: Implementations should have `#function` as the default value for this.
    ///     - line: Implementations should have `#line` as the default value for this.
    ///
    public func debug(_ message: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:], file: StaticString = #filePath, function: StaticString = #function, line: UInt = #line) {
        Task {
            guard await log.debugLoggingEnabled else {
                return
            }

            writeToUnifiedLoggingSystem(level: .debug, message: message, details: details, file: file, function: function, line: line)
            await log.write(category: category, level: .debug, message: message, details: details, file: file, function: function, line: line)
        }
    }

    ///
    /// Dispatch a task to write a message with the level `OSLogType.info`.
    ///
    /// - Parameters:
    ///     - message: A human-readable message, preferably generic and without interpolations. The `details` argument is for arguments.
    ///     - details: Structured and contextual details about a message.
    ///     - file: Implementations should have `#filePath` as the default value for this.
    ///     - function: Implementations should have `#function` as the default value for this.
    ///     - line: Implementations should have `#line` as the default value for this.
    ///
    public func info(_ message: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:], file: StaticString = #filePath, function: StaticString = #function, line: UInt = #line) {
        Task {
            writeToUnifiedLoggingSystem(level: .info, message: message, details: details, file: file, function: function, line: line)
            await log.write(category: category, level: .info, message: message, details: details, file: file, function: function, line: line)
        }
    }

    ///
    /// Dispatch a task to write a message with the level `OSLogType.error`.
    ///
    /// - Parameters:
    ///     - message: A human-readable message, preferably generic and without interpolations. The `details` argument is for arguments.
    ///     - details: Structured and contextual details about a message.
    ///     - file: Implementations should have `#filePath` as the default value for this.
    ///     - function: Implementations should have `#function` as the default value for this.
    ///     - line: Implementations should have `#line` as the default value for this.
    ///
    public func error(_ message: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:], file: StaticString = #filePath, function: StaticString = #function, line: UInt = #line) {
        Task {
            writeToUnifiedLoggingSystem(level: .error, message: message, details: details, file: file, function: function, line: line)
            await log.write(category: category, level: .error, message: message, details: details, file: file, function: function, line: line)
        }
    }

    ///
    /// Dispatch a task to write a message with the level `OSLogType.fault`.
    ///
    /// - Parameters:
    ///     - message: A human-readable message, preferably generic and without interpolations. The `details` argument is for arguments.
    ///     - details: Structured and contextual details about a message.
    ///     - file: Implementations should have `#filePath` as the default value for this.
    ///     - function: Implementations should have `#function` as the default value for this.
    ///     - line: Implementations should have `#line` as the default value for this.
    ///
    public func fault(_ message: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:], file: StaticString = #filePath, function: StaticString = #function, line: UInt = #line) {
        Task {
            writeToUnifiedLoggingSystem(level: .fault, message: message, details: details, file: file, function: function, line: line)
            await log.write(category: category, level: .fault, message: message, details: details, file: file, function: function, line: line)
        }
    }

    private func writeToUnifiedLoggingSystem(level: OSLogType, message: String, details: [FileProviderLogDetailKey: (any Sendable)?], file: StaticString, function: StaticString, line: UInt) {
        if details.isEmpty {
            logger.log(level: level, "\(message, privacy: .public)\n\n\(file, privacy: .public):\(line, privacy: .public) \(function, privacy: .public)")
            return
        }

        let sortedKeys = details.keys.sorted()

        let detailDescriptions: [String] = sortedKeys.compactMap { key in
            let valueDescription: String = switch details[key] {
                case let account as Account:
                    account.ncKitAccount
                case let date as Date:
                    date.ISO8601Format()
                case let error as NSError:
                    error.debugDescription
                case let lock as NKLock:
                    lock.token ?? "nil"
                case let item as NSFileProviderItem:
                    item.itemIdentifier.rawValue
                case let identifier as NSFileProviderItemIdentifier:
                    identifier.rawValue
                case let url as URL:
                    url.absoluteString
                case let text as String:
                    text
                case let request as NSFileProviderRequest:
                    "requestingExecutable: \(request.requestingExecutable?.path ?? "nil"), isFileViewerRequest: \(request.isFileViewerRequest), isSystemRequest: \(request.isSystemRequest)"
                case nil:
                    "nil"
                default:
                    "<unsupported log detail value type>"
            }

            return "- \(key.rawValue): \(valueDescription)"
        }

        logger.log(level: level, "\(message, privacy: .public)\n\n\(detailDescriptions.joined(separator: "\n"), privacy: .public)\n\n\(file, privacy: .public):\(line, privacy: .public) \(function, privacy: .public)")
    }
}
