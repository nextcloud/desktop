//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

///
/// Different schema versions shipped with this project.
///
enum SchemaVersion: UInt64 {
    case initial = 100
    case deletedLocalFileMetadata = 200
    case addedLockTokenPropertyToRealmItemMetadata = 201
    case addedIsLockFileOfLocalOriginToRealmItemMetadata = 202
}
