//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import os

///
/// A logging facility designed for file provider extensions. It writes JSON lines to a file.
///
/// Do not use this directly but create ``FileProviderLogger`` instances based on this instead.
///
/// Due to the dependency on the `NSFileProviderDomainIdentifier`, This needs to be intialized by the type implementing the `NSFileProviderReplicatedExtension`.
/// An instance would not be abled to resolve the file provider domain it is used by.
///
/// In general, there should be only one instance for every process.
///
/// This actor owns the JSON Lines log file. Forwarding to Apple's unified logging system is handled by ``FileProviderLogger``, which tags each message with the calling type's category.
///
/// Whether `.debug`-level messages are included is controlled at runtime by the `debugLoggingEnabled` boolean key under the file provider extension's domain in `UserDefaults.standard`.
/// When the key is unset, debug logging is enabled in DEBUG builds and disabled in release builds, matching the previous compile-time behavior.
/// The value is observed via KVO, so changes made with `defaults write` take effect without restarting the extension.
/// The gate applies to both the JSONL output here and the unified logging forwarding done by ``FileProviderLogger``.
/// See `Logging.md` for administrator instructions.
///
/// > To Do: Consider using macros for the calls so the calling functions and files can be recorded, too!
///
public actor FileProviderLog: FileProviderLogging {
    ///
    /// The file provider domain identifier for creating new log files during rotation.
    ///
    let domainIdentifier: NSFileProviderDomainIdentifier

    ///
    /// JSON encoder for ``FileProviderLogMessage`` values.
    ///
    let encoder: JSONEncoder

    ///
    /// The current file location to write messages to.
    ///
    var file: URL?

    ///
    /// The file manager to use for file system operations.
    ///
    let fileManager: FileManager

    ///
    /// Used for the file name part of the log files.
    ///
    let fileDateFormatter: DateFormatter

    ///
    /// Used for for the date strings in encoded ``FileProviderLogMessage``.
    ///
    let messageDateFormatter: DateFormatter

    ///
    /// The handle used for writing to the file located by ``url``.
    ///
    var handle: FileHandle?

    ///
    /// Unified logger used for self-diagnostics of this actor, tagged with the category `"FileProviderLog"`.
    ///
    /// Used for messages about this actor's own concerns — log file rotation, startup and KVO transitions of the debug-logging gate, and errors that prevent writing to the JSONL file.
    /// Messages emitted by callers are forwarded to Apple's unified logging system by ``FileProviderLogger`` under the respective calling type's category instead.
    ///
    let logger: Logger

    ///
    /// The logs directory where log files are stored.
    ///
    let logsDirectory: URL?

    ///
    /// Maximum log file size in bytes (100 MB).
    ///
    let maxLogFileSize: Int64 = 100 * 1024 * 1024

    ///
    /// The subsystem string to be used as with the unified logging system.
    ///
    let subsystem: String

    ///
    /// Whether `.debug`-level messages are included in both the JSONL file output and Apple unified logging.
    ///
    /// Initialized from `UserDefaults.standard` (key `"debugLoggingEnabled"`) at startup and kept in sync via KVO so changes propagate live.
    /// When the key is unset, falls back to the build-configuration default: enabled in DEBUG builds, disabled otherwise.
    ///
    public var debugLoggingEnabled: Bool

    ///
    /// Retains the KVO token that reacts to changes of the `debugLoggingEnabled` user default.
    ///
    /// Marked `nonisolated(unsafe)` so it can be assigned during `init` (after `self` escapes via the observer closure) and read from the non-isolated `deinit`.
    /// Safe in practice because it is written exactly once in `init` before the instance is visible to any other thread and read only in `deinit` after all other references have been dropped.
    ///
    nonisolated(unsafe) var debugLoggingObservation: NSKeyValueObservation?

    ///
    /// Initialize a new log file abstraction.
    ///
    /// - Parameters:
    ///     - fileProviderDomainIdentifier: The raw string value of the file provider domain which this file provider extension process is managing.
    ///
    public init(fileProviderDomainIdentifier identifier: NSFileProviderDomainIdentifier) {
        domainIdentifier = identifier
        encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        fileManager = FileManager.default

        fileDateFormatter = DateFormatter()
        fileDateFormatter.locale = Locale(identifier: "en_US_POSIX")
        fileDateFormatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"

        messageDateFormatter = DateFormatter()
        messageDateFormatter.locale = Locale(identifier: "en_US_POSIX")
        messageDateFormatter.dateFormat = "yyyy.MM.dd HH:mm:ss.SSS"

        subsystem = Bundle.main.bundleIdentifier ?? ""
        logger = Logger(subsystem: subsystem, category: "FileProviderLog")

        let defaultFallback = Self.buildConfigurationDefaultDebugLoggingEnabled
        let raw = UserDefaults.standard.object(forKey: "debugLoggingEnabled")

        if raw == nil {
            debugLoggingEnabled = defaultFallback
            logger.log(level: .default, "Debug logging \(defaultFallback ? "enabled" : "disabled", privacy: .public) (build fallback; `debugLoggingEnabled` default is unset).")
        } else if let number = raw as? NSNumber {
            debugLoggingEnabled = number.boolValue
            logger.log(level: .default, "Debug logging \(number.boolValue ? "enabled" : "disabled", privacy: .public) (from `debugLoggingEnabled` user default).")
        } else {
            debugLoggingEnabled = defaultFallback
            logger.error("Ignoring non-boolean value for `debugLoggingEnabled` user default. Falling back to build default (\(defaultFallback ? "enabled" : "disabled", privacy: .public)).")
        }

        guard let logsDirectory = fileManager.fileProviderDomainLogDirectory(for: identifier) else {
            logger.error("Failed to get URL for file provider domain logs!")
            file = nil
            handle = nil
            logsDirectory = nil
            debugLoggingObservation = UserDefaults.standard.observe(\.debugLoggingEnabled, options: [.new]) { [weak self] _, _ in
                Task { [weak self] in
                    await self?.reloadDebugLoggingEnabled()
                }
            }
            return
        }

        self.logsDirectory = logsDirectory
        debugLoggingObservation = UserDefaults.standard.observe(\.debugLoggingEnabled, options: [.new]) { [weak self] _, _ in
            Task { [weak self] in
                await self?.reloadDebugLoggingEnabled()
            }
        }
    }

    deinit {
        debugLoggingObservation?.invalidate()
    }

    ///
    /// Build-configuration default for ``debugLoggingEnabled``, used when the user default is unset.
    ///
    private static var buildConfigurationDefaultDebugLoggingEnabled: Bool {
        #if DEBUG
            return true
        #else
            return false
        #endif
    }

    ///
    /// Re-reads the `debugLoggingEnabled` user default and updates ``debugLoggingEnabled`` accordingly.
    ///
    /// Called from the KVO observer when `defaults write` (or `defaults delete`) changes the value in another process.
    ///
    func reloadDebugLoggingEnabled() {
        let defaultFallback = Self.buildConfigurationDefaultDebugLoggingEnabled
        let raw = UserDefaults.standard.object(forKey: "debugLoggingEnabled")
        let newValue: Bool
        let source: String

        if raw == nil {
            newValue = defaultFallback
            source = "build fallback; `debugLoggingEnabled` default is unset"
        } else if let number = raw as? NSNumber {
            newValue = number.boolValue
            source = "from `debugLoggingEnabled` user default"
        } else {
            newValue = defaultFallback
            logger.error("Ignoring non-boolean value for `debugLoggingEnabled` user default. Falling back to build default.")
            source = "build fallback; `debugLoggingEnabled` default is of an unsupported type"
        }

        if newValue == debugLoggingEnabled {
            return
        }

        debugLoggingEnabled = newValue
        logger.log(level: .default, "Debug logging \(newValue ? "enabled" : "disabled", privacy: .public) (\(source, privacy: .public)).")
    }

    ///
    /// Rotates the log file by creating a new one when the current file becomes too large.
    ///
    func rotateLogFileIfNeeded() {
        guard let logsDirectory else {
            logger.error("Cancelling log file rotation due to the lack of a logs directory!")
            return
        }

        do {
            if let currentFile = file {
                let fileAttributes = try fileManager.attributesOfItem(atPath: currentFile.path)

                if let fileSize = fileAttributes[.size] as? Int64, fileSize >= maxLogFileSize {
                    // Close current handle
                    handle?.closeFile()
                    logger.debug("Closed current log file at \"\(currentFile.path, privacy: .public)\" because it exceeds the size limit.")

                    file = nil
                    handle = nil
                }
            }
        } catch {
            // swiftformat:disable:next redundantSelf
            logger.error("Failed to close open log file at \"\(self.file?.path ?? "nil")\": \(error.localizedDescription, privacy: .public)")
        }

        guard handle == nil else {
            // Already have an active handle which was not closed previously, stick with that file.
            return
        }

        let creationDate = Date()
        let formattedDate = fileDateFormatter.string(from: creationDate)
        let processIdentifier = ProcessInfo.processInfo.processIdentifier
        let name = "\(formattedDate) (\(processIdentifier)).jsonl"
        let newFile = logsDirectory.appendingPathComponent(name, isDirectory: false)

        if fileManager.createFile(atPath: newFile.path, contents: nil) == false {
            logger.error("Failed to create new log file at: \"\(newFile.path, privacy: .public)\".")
            return
        } else {
            logger.debug("Created new log file at: \"\(newFile.path, privacy: .public)\".")
        }

        do {
            file = newFile
            handle = try FileHandle(forWritingTo: newFile)
            logger.debug("Opened new log file for writing at: \"\(newFile.path, privacy: .public)\".")
        } catch {
            logger.error("Failed to open new log file at \"\(newFile.path, privacy: .public)\" for writing: \(error.localizedDescription, privacy: .public)")
        }

        // Clean up old log files (older than 24 hours)
        cleanupOldLogFiles()
    }

    ///
    /// Removes log files that are older than 24 hours, excluding the current active log file.
    ///
    private func cleanupOldLogFiles() {
        guard let logsDirectory else {
            logger.error("Cannot cleanup old log files: logs directory is nil")
            return
        }

        do {
            let contents = try fileManager.contentsOfDirectory(at: logsDirectory, includingPropertiesForKeys: [.creationDateKey], options: [.skipsHiddenFiles])
            let currentTime = Date()
            let twentyFourHoursAgo = currentTime.addingTimeInterval(-24 * 60 * 60)

            for fileURL in contents {
                // Skip if this is the current active log file
                if let currentFile = file, fileURL == currentFile {
                    continue
                }

                // Only process .jsonl files
                guard fileURL.pathExtension == "jsonl" else {
                    continue
                }

                do {
                    let resourceValues = try fileURL.resourceValues(forKeys: [.creationDateKey])

                    if let creationDate = resourceValues.creationDate, creationDate < twentyFourHoursAgo {
                        try fileManager.removeItem(at: fileURL)
                        logger.debug("Deleted old log file: \"\(fileURL.path, privacy: .public)\" (created: \(creationDate, privacy: .public))")
                    }
                } catch {
                    logger.error("Failed to delete old log file at \"\(fileURL.path)\": \(error.localizedDescription, privacy: .public)")
                }
            }
        } catch {
            logger.error("Failed to enumerate log files for cleanup: \(error.localizedDescription, privacy: .public)")
        }
    }

    public func write(category: String, level: OSLogType, message: String, details: [FileProviderLogDetailKey: (any Sendable)?], file: StaticString, function: StaticString, line: UInt) {
        if level == .debug, !debugLoggingEnabled {
            return
        }

        rotateLogFileIfNeeded() // Check if log file needs rotation before writing anything.

        guard let handle else { // Continue only when a file handle is available.
            return
        }

        let levelDescription = switch level {
            case .debug:
                "debug"
            case .info:
                "info"
            case .default:
                "default"
            case .error:
                "error"
            case .fault:
                "fault"
            default:
                "default"
        }

        let date = Date()
        let formattedDate = messageDateFormatter.string(from: date)
        let entry = FileProviderLogMessage(category: category, date: formattedDate, details: details, level: levelDescription, message: message, file: file, function: function, line: line)

        do {
            let object = try encoder.encode(entry)
            try handle.write(contentsOf: object)
            try handle.write(contentsOf: "\n".data(using: .utf8)!)
            try handle.synchronize()
        } catch {
            logger.error("Failed to encode and write message: \(message, privacy: .public)!")
            return
        }
    }
}
