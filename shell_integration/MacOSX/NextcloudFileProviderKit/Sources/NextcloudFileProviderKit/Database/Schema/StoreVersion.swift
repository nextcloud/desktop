//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

///
/// Generation of the on-disk format of the metadata database.
///
/// Bump it together with every migration registered in ``DatabaseSchema``. It is recorded in the file as `PRAGMA user_version` and in the domain's defaults, so a build which meets a newer store than it understands can set the store up from scratch instead of misreading it.
///
enum StoreVersion {
    static let current = 1
}
