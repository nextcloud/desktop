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

#ifndef SYNCFILESTATUS_H
#define SYNCFILESTATUS_H

#include <QString>

#include "owncloudlib.h"

namespace OCC {

/**
 * @brief The SyncFileStatus class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SyncFileStatus
{
public:
    enum SyncFileStatusTag {
        STATUS_NONE,
        STATUS_EVAL,
        STATUS_REMOVE,
        STATUS_RENAME,
        STATUS_MOVE,
        STATUS_NEW,
        STATUS_CONFLICT,
        STATUS_IGNORE,
        STATUS_SYNC,
        STATUS_STAT_ERROR,
        STATUS_ERROR,
        STATUS_UPDATED
    };

    SyncFileStatus();
    SyncFileStatus(SyncFileStatusTag);

    void set(SyncFileStatusTag tag);
    SyncFileStatusTag tag();

    void setSharedWithMe( bool isShared );
    bool sharedWithMe();

    QString toSocketAPIString() const;
private:
    SyncFileStatusTag _tag;
    bool _sharedWithMe;

};
}

#endif // SYNCFILESTATUS_H
