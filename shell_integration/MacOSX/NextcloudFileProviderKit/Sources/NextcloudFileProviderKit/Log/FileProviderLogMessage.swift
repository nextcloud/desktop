//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// A data model for the rich JSON object to be written into the JSON lines log files.
///
public struct FileProviderLogMessage: Encodable {
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
    /// Textual representation of the associated `OSLogType`.
    ///
    public let level: String

    ///
    /// The actual text for the entry.
    ///
    public let message: String

    ///
    /// Custom initializer to support arbitrary types as detail values.
    ///
    init(category: String, date: String, details: [FileProviderLogDetailKey: Any?], level: String, message: String) {
        self.category = category
        self.date = date

        var transformedDetails = [String: FileProviderLogDetail?]()

        for key in details.keys {
            transformedDetails[key.rawValue] = FileProviderLogDetail(details[key] as Any?)
        }

        self.details = transformedDetails
        self.level = level
        self.message = message
    }
}
