/*
 * Copyright (C) by Roeland Jago Douma <rullzer@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef SHAREPERMISSIONS_H
#define SHAREPERMISSIONS_H

#include <qglobal.h>

namespace OCC {

/**
 * Possible permissions, must match the server permission constants
 */
enum SharePermission {
    SharePermissionRead     = 1 << 0,
    SharePermissionUpdate   = 1 << 1,
    SharePermissionCreate   = 1 << 2,
    SharePermissionDelete   = 1 << 3,
    SharePermissionShare    = 1 << 4,
    SharePermissionDefault  = 1 << 30
};
Q_DECLARE_FLAGS(SharePermissions, SharePermission)
Q_DECLARE_OPERATORS_FOR_FLAGS(SharePermissions)

} // namespace OCC

#endif
