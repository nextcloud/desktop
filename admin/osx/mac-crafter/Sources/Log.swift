// SPDX-FileCopyrightText: Nextcloud GmbH
// SPDX-FileCopyrightText: 2026 Iva Horn
// SPDX-License-Identifier: GPL-2.0-or-later

import Foundation
import os

///
/// A simple logging facility for Mac Crafter to abstract terminal output, log file and unified logging system.
///
actor Log: Sendable {
    private static let shared = Log()
    private let handle: FileHandle?

    init() {
        let manager = FileManager.default

        if let directory = try? manager.url(for: .libraryDirectory, in: .userDomainMask, appropriateFor: nil, create: true).appendingPathComponent("Logs").appendingPathComponent("mac-crafter") {
                if manager.fileExists(atPath: directory.path) == false {
                try? manager.createDirectory(at: directory, withIntermediateDirectories: true)
            }

            let file = directory
                .appendingPathComponent(UUID().uuidString)
                .appendingPathExtension("log")

            manager.createFile(atPath: file.path, contents: nil)
            handle = try? FileHandle(forWritingTo: file)
        } else {
            handle = nil
        }
    }

    private func log(level: OSLogType, message: String) {
        guard message.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty == false else {
            return
        }

        print(message)

        guard let handle else {
            return
        }

        guard let data = "\(message)\n".data(using: .utf8) else {
            return
        }

        try? handle.write(contentsOf: data)
    }

    ///
    /// Write an informative message.
    ///
    static func info(_ message: String) {
        Task {
            await shared.log(level: .info, message: message)
        }
    }

    ///
    /// Write an error message.
    ///
    static func error(_ message: String) {
        Task {
            await shared.log(level: .error, message: message)
        }
    }
}
