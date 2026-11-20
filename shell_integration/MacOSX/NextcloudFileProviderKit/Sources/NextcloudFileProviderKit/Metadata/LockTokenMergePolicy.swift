//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

enum LockTokenMergePolicy: Sendable {
    case preserveLocal
    case useIncoming
    case clear
}
