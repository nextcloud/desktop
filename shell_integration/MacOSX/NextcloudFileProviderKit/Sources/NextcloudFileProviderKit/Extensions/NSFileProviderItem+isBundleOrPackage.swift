//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import UniformTypeIdentifiers

extension NSFileProviderItemProtocol {
    ///
    /// Determine whether the file provider item is a package or a bundle.
    ///
    var isBundleOrPackage: Bool {
        if contentType?.conforms(to: .bundle) == true {
            return true
        }

        if contentType?.conforms(to: .package) == true {
            return true
        }

        let fileExtension = filename.fileExtension

        if fileExtension.isEmpty == false {
            if let type = UTType(filenameExtension: fileExtension) {
                if type.conforms(to: .applicationBundle) {
                    return true
                }

                if type.conforms(to: .application) {
                    return true
                }
            }
        }

        return false
    }
}
