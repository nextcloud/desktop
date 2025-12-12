//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import os

///
/// A proxy type to be used by logging types.
///
/// - Automatically augments messages with the once configured category.
/// - Translates the designated methods to the log level argument.
/// - Provides synchronous methods to dispatch log messages to the underlying actor.
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
    /// Create a new logger which is supposed to be used by individual types and their instances.
    ///
    public init(category: String, log: any FileProviderLogging) {
        self.category = category
        self.log = log
    }

    ///
    /// Dispatch a task to write a message with the level `OSLogType.debug`.
    ///
    /// - Parameters:
    ///     - message: The main text message of the entry in the logs.
    ///     - details: Additional contextual data.
    ///
    public func debug(_ message: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:]) {
        Task {
            await log.write(category: category, level: .debug, message: message, details: details)
        }
    }

    ///
    /// Dispatch a task to write a message with the level `OSLogType.info`.
    ///
    /// - Parameters:
    ///     - message: The main text message of the entry in the logs.
    ///     - details: Additional contextual data.
    ///
    public func info(_ message: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:]) {
        Task {
            await log.write(category: category, level: .info, message: message, details: details)
        }
    }

    ///
    /// Dispatch a task to write a message with the level `OSLogType.error`.
    ///
    /// - Parameters:
    ///     - message: The main text message of the entry in the logs.
    ///     - details: Additional contextual data.
    ///
    public func error(_ message: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:]) {
        Task {
            await log.write(category: category, level: .error, message: message, details: details)
        }
    }

    ///
    /// Dispatch a task to write a message with the level `OSLogType.fault`.
    ///
    /// - Parameters:
    ///     - message: The main text message of the entry in the logs.
    ///     - details: Additional contextual data.
    ///
    public func fault(_ message: String, _ details: [FileProviderLogDetailKey: (any Sendable)?] = [:]) {
        Task {
            await log.write(category: category, level: .fault, message: message, details: details)
        }
    }
}
