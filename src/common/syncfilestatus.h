/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef SYNCFILESTATUS_H
#define SYNCFILESTATUS_H

#include <QMetaType>
#include <QObject>
#include <QString>

#include "ocsynclib.h"

namespace OCC {

/**
 * @brief The SyncFileStatus class
 * @ingroup libsync
 */
class OCSYNC_EXPORT SyncFileStatus
{
public:
    enum SyncFileStatusTag {
        StatusNone,
        StatusSync,
        StatusWarning,
        StatusUpToDate,
        StatusError,
        StatusExcluded,
    };

    SyncFileStatus();
    SyncFileStatus(SyncFileStatusTag);

    void set(SyncFileStatusTag tag);
    [[nodiscard]] SyncFileStatusTag tag() const;

    void setShared(bool isShared);
    [[nodiscard]] bool shared() const;

    [[nodiscard]] QString toSocketAPIString() const;

private:
    SyncFileStatusTag _tag = StatusNone;
    bool _shared = false;
};

inline bool operator==(const SyncFileStatus &a, const SyncFileStatus &b)
{
    return a.tag() == b.tag() && a.shared() == b.shared();
}

inline bool operator!=(const SyncFileStatus &a, const SyncFileStatus &b)
{
    return !(a == b);
}
}

Q_DECLARE_METATYPE(OCC::SyncFileStatus)

#endif // SYNCFILESTATUS_H
