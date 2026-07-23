//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

extension String {
    /// Returns the canonical form used for local File Provider identity comparisons.
    var canonicalForm: String {
        precomposedStringWithCanonicalMapping
    }
}
