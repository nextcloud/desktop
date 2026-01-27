//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

extension NSFileProviderItem {
    ///
    /// Parse the original file name of a lock file name.
    ///
    /// - Example for Microsoft Office: `MyDoc.docx` is extracted from `~$MyDoc.docx`.
    /// - Example for LibreOffice: `MyDoc.odt` is extracted from `.~lock.MyDoc.odt#`.
    /// - Filename with less than 8 characters like `Test.docx` will result in a lock file named `~$Test.docx`.
    /// - Filename with more than 8 characters like `Document.docx` will result in a lock file named `~$cument.docx`.
    /// - Filename sandbox-style temporary naming like `Welcome123456.doc.sb-d215eb53-IBAwfU`.
    ///
    /// - Returns: Either the original file name parsed from the given lock file name or `nil`, if it is not a recognized lock file format.
    ///
    public func lockedFileName() -> String? {
        // Microsoft Office
        if filename.hasPrefix("~$") {
            let index = filename.index(filename.startIndex, offsetBy: 2)
            return String(filename[index...])
        }

        // LibreOffice and OnlyOffice
        if filename.hasPrefix(".~lock."), filename.hasSuffix("#") {
            let start = filename.index(filename.startIndex, offsetBy: 7)
            let end = filename.index(before: filename.endIndex)
            return String(filename[start ..< end])
        }

        // Sandbox
        if let sbRange = filename.range(of: ".sb-") {
            return String(filename[..<sbRange.lowerBound])
        }

        return nil
    }
}
