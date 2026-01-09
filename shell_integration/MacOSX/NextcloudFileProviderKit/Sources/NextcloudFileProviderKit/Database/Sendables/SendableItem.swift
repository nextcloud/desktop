//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider

///
/// A value type representation of persisted item metadata which is safe to pass across concurrency domains.
///
public struct SendableItem: Sendable {
    let fileProviderItemIdentifier: NSFileProviderItemIdentifier
    let name: String
}
