//
//  URL+Extensions.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 21/4/25.
//

import Foundation

fileprivate let invalidCharacters = CharacterSet(charactersIn: "<>:/\\|?*\" .") // Include " " and .
fileprivate let invalidCharacterReplacement = "_"
fileprivate let invalidCharacterReplacementSet =
    CharacterSet(charactersIn: invalidCharacterReplacement)

/// Sanitises a string by replacing invalid characters with underscores.
///
/// This helper function takes a string and replaces any characters not allowed in filenames
/// with underscores.  It also removes leading/trailing underscores and collapses multiple
/// consecutive underscores into a single underscore.
///
/// - Parameter string: The string to sanitise.
/// - Returns: The sanitised string.
///
fileprivate func sanitise(string: String) -> String {
    string
        .components(separatedBy: invalidCharacters)
        .joined(separator: invalidCharacterReplacement)
        .trimmingCharacters(in: invalidCharacterReplacementSet) // Remove leading/trailing
        .replacingOccurrences( // Replace multiple consecutive replacement chars
            of: "\(invalidCharacterReplacement){2,}",
            with: invalidCharacterReplacement,
            options: .regularExpression
        )
}

public extension URL {
    func safeFilenameFromURLString(
        defaultingTo defaultFilename: String = "default_filename"
    ) -> String {
        let host = host ?? ""
        let query = query ?? ""

        let sanitisedHost = sanitise(string: host)
        var sanitisedPath = sanitise(string: path)
        if sanitisedPath.hasPrefix("/") {
            sanitisedPath.removeFirst()
        }
        let sanitisedQuery = sanitise(string: query)
        let filename = "\(sanitisedHost)_\(sanitisedPath)_\(sanitisedQuery)"
        return sanitise(string: filename)
    }
}
