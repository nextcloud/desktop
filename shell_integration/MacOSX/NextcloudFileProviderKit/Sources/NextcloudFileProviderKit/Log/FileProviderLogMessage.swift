//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// A data model for the rich JSON object to be written into the JSON lines log files.
///
public struct FileProviderLogMessage: Encodable {
    enum CodingKeys: CodingKey {
        case category
        case date
        case details
        case file
        case function
        case level
        case line
        case message
    }

    ///
    /// As used with `Logger` of the `os` framework.
    ///
    public let category: String

    ///
    /// Time of the message to write.
    ///
    public let date: String

    ///
    /// An optional dictionary of additional metadata related to the message.
    ///
    /// This is intended to improve the filter possibilities in logs by structuring the messages in more detail.
    /// By providing contextual identifiers, the generic stream of messages can be filtered centered around individual subjects like file provider items or similar.
    ///
    public let details: [String: FileProviderLogDetail?]

    ///
    /// The source code file which generates the log message.
    ///
    public let file: String?

    ///
    /// The calling function generating this message.
    ///
    public let function: String?

    ///
    /// Textual representation of the associated `OSLogType`.
    ///
    public let level: String

    ///
    /// The line in the source code file which generates this message.
    ///
    public let line: UInt?

    ///
    /// The actual text for the entry.
    ///
    public let message: String

    ///
    /// Custom initializer to support arbitrary types as detail values.
    ///
    init(category: String, date: String, details: [FileProviderLogDetailKey: Any?], level: String, message: String, file: StaticString, function: StaticString, line: UInt) {
        self.category = category
        self.date = date

        var transformedDetails = [String: FileProviderLogDetail?]()

        for key in details.keys {
            transformedDetails[key.rawValue] = FileProviderLogDetail(details[key] as Any?)
        }

        self.details = transformedDetails
        self.level = level
        self.message = message

        #if DEBUG
            self.file = String("\(file)")
            self.function = String("\(function)")
            self.line = line
        #else
            self.file = nil
            self.function = nil
            self.line = nil
        #endif
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(category, forKey: .category)
        try container.encode(date, forKey: .date)
        try container.encode(details, forKey: .details)
        try container.encode(level, forKey: .level)
        try container.encode(message, forKey: .message)

        #if DEBUG
            try container.encode(file, forKey: .file)
            try container.encode(function, forKey: .function)
            try container.encode(line, forKey: .line)
        #endif
    }
}
