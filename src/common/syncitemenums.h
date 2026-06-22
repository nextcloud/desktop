/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYNCITEMENUMS_H
#define SYNCITEMENUMS_H

#include "ocsynclib.h"

#include <QObject>

namespace OCC {

namespace SyncFileItemEnums {

OCSYNC_EXPORT Q_NAMESPACE

enum class LockStatus {
    UnlockedItem = 0,
    LockedItem = 1,
};

Q_ENUM_NS(LockStatus)

enum class LockOwnerType : int{
    UserLock = 0,
    AppLock = 1,
    TokenLock = 2,
};

Q_ENUM_NS(LockOwnerType)

}

}

#endif // SYNCITEMENUMS_H
