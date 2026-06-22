/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "syncfilestatus.h"

namespace OCC {
SyncFileStatus::SyncFileStatus() = default;

SyncFileStatus::SyncFileStatus(SyncFileStatusTag tag)
    : _tag(tag)
{
}

void SyncFileStatus::set(SyncFileStatusTag tag)
{
    _tag = tag;
}

SyncFileStatus::SyncFileStatusTag SyncFileStatus::tag() const
{
    return _tag;
}

void SyncFileStatus::setShared(bool isShared)
{
    _shared = isShared;
}

bool SyncFileStatus::shared() const
{
    return _shared;
}

QString SyncFileStatus::toSocketAPIString() const
{
    QString statusString;
    bool canBeShared = true;

    switch (_tag) {
    case StatusNone:
        statusString = QStringLiteral("NOP");
        canBeShared = false;
        break;
    case StatusSync:
        statusString = QStringLiteral("SYNC");
        break;
    case StatusWarning:
        // The protocol says IGNORE, but all implementations show a yellow warning sign.
        statusString = QStringLiteral("IGNORE");
        break;
    case StatusUpToDate:
        statusString = QStringLiteral("OK");
        break;
    case StatusError:
        statusString = QStringLiteral("ERROR");
        break;
    case StatusExcluded:
        // The protocol says IGNORE, but all implementations show a yellow warning sign.
        statusString = QStringLiteral("IGNORE");
        break;
    }
    if (canBeShared && _shared) {
        statusString += QLatin1String("+SWM");
    }

    return statusString;
}
}
