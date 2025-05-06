/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
