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

namespace OCC {
namespace Version {
    /**
  * "Major.Minor.Patch"
  */
    QString OCSYNC_EXPORT string();
    int OCSYNC_EXPORT major();
    int OCSYNC_EXPORT minor();
    int OCSYNC_EXPORT patch();
    int OCSYNC_EXPORT buildNumber();

    /**
 * git, rc1, rc2
 * Empty in releases
 */
    QString OCSYNC_EXPORT suffix();
    /**
 * The commit id
 */
    QString OCSYNC_EXPORT gitSha();
}

}
