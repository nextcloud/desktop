/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SHAREPERMISSIONS_H
#define SHAREPERMISSIONS_H

#include <qglobal.h>

namespace OCC {

/**
 * Possible permissions, must match the server permission constants
 */
enum SharePermission {
    SharePermissionRead = 1,
    SharePermissionUpdate = 1 << 1,
    SharePermissionCreate = 1 << 2,
    SharePermissionDelete = 1 << 3,
    SharePermissionShare = 1 << 4,
    SharePermissionAll = 31,
};
Q_DECLARE_FLAGS(SharePermissions, SharePermission)
Q_DECLARE_OPERATORS_FOR_FLAGS(SharePermissions)

} // namespace OCC

Q_DECLARE_METATYPE(OCC::SharePermission)
Q_DECLARE_METATYPE(OCC::SharePermissions)

#endif
