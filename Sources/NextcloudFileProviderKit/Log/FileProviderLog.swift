import FileProvider
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
/// > To Do: Consider using macros for the calls so the calling functions and files can be recorded, too!
///
/// > To Do: Implement log rotation!
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
    /// Used for for the date strings in encoded ``FileProviderLogMessage``.
    ///
    let formatter: DateFormatter

    ///
    /// The handle used for writing to the file located by ``url``.
    ///
    var handle: FileHandle?

    ///
    /// The fallback logger.
    ///
    /// This is important in case the actual log file could be created or written to.
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
    /// Initialize a new log file abstraction.
    ///
    /// - Parameters:
    ///     - fileProviderDomainIdentifier: The raw string value of the file provider domain which this file provider extension process is managing.
    ///
    public init(fileProviderDomainIdentifier identifier: NSFileProviderDomainIdentifier) {
        self.domainIdentifier = identifier
        self.encoder = JSONEncoder()
        self.encoder.outputFormatting = [.sortedKeys]
        self.fileManager = FileManager.default

        self.formatter = DateFormatter()
        self.formatter.locale = Locale(identifier: "en_US_POSIX")
        self.formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"

        self.subsystem = Bundle.main.bundleIdentifier ?? ""
        self.logger = Logger(subsystem: self.subsystem, category: "FileProviderLog")

        guard let logsDirectory = fileManager.fileProviderDomainLogDirectory(for: identifier) else {
            logger.error("Failed to get URL for file provider domain logs!")
            self.file = nil
            self.handle = nil
            self.logsDirectory = nil
            return
        }

        self.logsDirectory = logsDirectory
    }

    ///
    /// Rotates the log file by creating a new one when the current file becomes too large.
    ///
    func rotateLogFileIfNeeded() {
        guard let logsDirectory = logsDirectory else {
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

                    self.file = nil
                    self.handle = nil
                }
            }
        } catch {
            logger.error("Failed to close open log file at \"\(self.file?.path ?? "nil")\": \(error.localizedDescription, privacy: .public)")
        }

        guard handle == nil else {
            // Already have an active handle which was not closed previously, stick with that file.
            return
        }

        let creationDate = Date()
        let formattedDate = formatter.string(from: creationDate)
        let processIdentifier = ProcessInfo.processInfo.processIdentifier
        let name = "\(formattedDate) (\(processIdentifier)).jsonl"
        let newFile = logsDirectory.appendingPathComponent(name, isDirectory: false)

        if fileManager.createFile(atPath: newFile.path, contents: nil) == false {
            logger.error("Failed to create new log file at: \"\(newFile.path)\".")
            return
        } else {
            logger.debug("Created new log file at: \"\(newFile.path)\".")
        }

        do {
            self.file = newFile
            self.handle = try FileHandle(forWritingTo: newFile)
            logger.debug("Opened new log file for writing at: \"\(newFile.path)\".")
        } catch {
            logger.error("Failed to open new log file at \"\(newFile.path)\" for writing: \(error.localizedDescription, privacy: .public)")
        }

        // Clean up old log files (older than 24 hours)
        cleanupOldLogFiles()
    }

    ///
    /// Removes log files that are older than 24 hours, excluding the current active log file.
    ///
    private func cleanupOldLogFiles() {
        guard let logsDirectory = logsDirectory else {
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

    public func write(category: String, level: OSLogType, message: String, details: [FileProviderLogDetailKey: Any?]) {
        if handle == nil {
            let detailsDescription = details.map { key, value in
                "\(key.rawValue): \(value ?? "nil")\n"
            }

            if detailsDescription.isEmpty {
                logger.log(level: level, "\(message, privacy: .public)")
            } else {
                logger.log(level: level, "\(message, privacy: .public)\n\n\(detailsDescription)")
            }
        }

        rotateLogFileIfNeeded() // Check if log file needs rotation before writing anything.

        guard let handle else { // Continue only when a file handle is available.
            return
        }

        let levelDescription: String

        switch level {
            case .debug:
                levelDescription = "debug"
            case .info:
                levelDescription = "info"
            case .default:
                levelDescription = "default"
            case .error:
                levelDescription = "error"
            case .fault:
                levelDescription = "fault"
            default:
                levelDescription = "default"
                break
        }

        let date = Date()
        let formattedDate = formatter.string(from: date)
        let entry = FileProviderLogMessage(category: category, date: formattedDate, details: details, level: levelDescription, message: message, subsystem: subsystem)

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
