/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#pragma once

#include "ocsynclib.h"

#include <QString>
#include <QVersionNumber>

namespace OCC::Version {
OCSYNC_EXPORT const QVersionNumber &version();

OCSYNC_EXPORT const QVersionNumber &versionWithBuildNumber();

inline int buildNumber()
{
    return versionWithBuildNumber().segmentAt(3);
}

/**
 * git, rc1, rc2
 * Empty in releases
 */
OCSYNC_EXPORT QString suffix();

/**
 * The commit id
 */
OCSYNC_EXPORT QString gitSha();

OCSYNC_EXPORT QString displayString();
}
