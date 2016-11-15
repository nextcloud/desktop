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

#include <QMetaType>
#include <QObject>
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
        StatusNone,
        StatusSync,
        StatusWarning,
        StatusUpToDate,
        StatusError,
    };

    SyncFileStatus();
    SyncFileStatus(SyncFileStatusTag);

    void set(SyncFileStatusTag tag);
    SyncFileStatusTag tag() const;

    void setShared( bool isShared );
    bool shared() const;

    QString toSocketAPIString() const;
private:
    SyncFileStatusTag _tag;
    bool _shared;

};

inline bool operator==(const SyncFileStatus &a, const SyncFileStatus &b) {
    return a.tag() == b.tag() && a.shared() == b.shared();
}

inline bool operator!=(const SyncFileStatus &a, const SyncFileStatus &b) {
    return !(a == b);
}
}

Q_DECLARE_METATYPE(OCC::SyncFileStatus)

#endif // SYNCFILESTATUS_H
