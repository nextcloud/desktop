/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "syncfilestatus.h"

#include <QDebug>

namespace OCC {
SyncFileStatus::SyncFileStatus()
    :_tag(STATUS_NONE), _sharedWithMe(false)
{
}

SyncFileStatus::SyncFileStatus(SyncFileStatusTag tag)
    :_tag(tag), _sharedWithMe(false)
{

}

void SyncFileStatus::set(SyncFileStatusTag tag)
{
    _tag = tag;
}

SyncFileStatus::SyncFileStatusTag SyncFileStatus::tag()
{
    return _tag;
}

void SyncFileStatus::setSharedWithMe(bool isShared)
{
    _sharedWithMe = isShared;
}

bool SyncFileStatus::sharedWithMe()
{
    return _sharedWithMe;
}

QString SyncFileStatus::toSocketAPIString() const
{
    QString statusString;

    switch(_tag)
    {
    case STATUS_NONE:
        statusString = QLatin1String("NONE");
        break;
    case STATUS_EVAL:
        statusString = QLatin1String("SYNC");
        break;
    case STATUS_NEW:
        statusString = QLatin1String("NEW");
        break;
    case STATUS_IGNORE:
        statusString = QLatin1String("IGNORE");
        break;
    case STATUS_SYNC:
    case STATUS_UPDATED:
        statusString = QLatin1String("OK");
        break;
    case STATUS_STAT_ERROR:
    case STATUS_ERROR:
        statusString = QLatin1String("ERROR");
        break;
    default:
        qWarning() << "This status should not be here:" << _tag;
        Q_ASSERT(false);
        statusString = QLatin1String("NONE");
    }
    if(_sharedWithMe) {
        statusString += QLatin1String("+SWM");
    }

    return statusString;
}
}
