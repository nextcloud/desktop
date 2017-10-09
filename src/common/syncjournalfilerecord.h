/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef SYNCJOURNALFILERECORD_H
#define SYNCJOURNALFILERECORD_H

#include <QString>
#include <QDateTime>

#include "ocsynclib.h"
#include "remotepermissions.h"
#include "common/utility.h"

namespace OCC {

class SyncFileItem;

/**
 * @brief The SyncJournalFileRecord class
 * @ingroup libsync
 */
class OCSYNC_EXPORT SyncJournalFileRecord
{
public:
    SyncJournalFileRecord();

    bool isValid() const
    {
        return !_path.isEmpty();
    }

    /** Returns the numeric part of the full id in _fileId.
     *
     * On the server this is sometimes known as the internal file id.
     *
     * It is used in the construction of private links.
     */
    QByteArray numericFileId() const;
    QDateTime modDateTime() const { return Utility::qDateTimeFromTime_t(_modtime); }

    QByteArray _path;
    quint64 _inode;
    qint64 _modtime;
    int _type;
    QByteArray _etag;
    QByteArray _fileId;
    qint64 _fileSize;
    RemotePermissions _remotePerm;
    bool _serverHasIgnoredFiles;
    QByteArray _checksumHeader;
};

bool OCSYNC_EXPORT
operator==(const SyncJournalFileRecord &lhs,
    const SyncJournalFileRecord &rhs);

class OCSYNC_EXPORT SyncJournalErrorBlacklistRecord
{
public:
    enum Category {
        /// Normal errors have no special behavior
        Normal = 0,
        /// These get a special summary message
        InsufficientRemoteStorage
    };

    SyncJournalErrorBlacklistRecord()
        : _retryCount(0)
        , _errorCategory(Category::Normal)
        , _lastTryModtime(0)
        , _lastTryTime(0)
        , _ignoreDuration(0)
    {
    }

    /// The number of times the operation was unsuccessful so far.
    int _retryCount;

    /// The last error string.
    QString _errorString;
    /// The error category. Sometimes used for special actions.
    Category _errorCategory;

    qint64 _lastTryModtime;
    QByteArray _lastTryEtag;

    /// The last time the operation was attempted (in s since epoch).
    qint64 _lastTryTime;

    /// The number of seconds the file shall be ignored.
    qint64 _ignoreDuration;

    QString _file;
    QString _renameTarget;

    bool isValid() const;
};
}

#endif // SYNCJOURNALFILERECORD_H
