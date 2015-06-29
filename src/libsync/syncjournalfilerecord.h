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

#ifndef SYNCJOURNALFILERECORD_H
#define SYNCJOURNALFILERECORD_H

#include <QString>
#include <QDateTime>

#include "owncloudlib.h"

namespace OCC {

class SyncFileItem;

/**
 * @brief The SyncJournalFileRecord class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SyncJournalFileRecord
{
public:
    SyncJournalFileRecord();
    SyncJournalFileRecord(const SyncFileItem&, const QString &localFileName);

    bool isValid() {
        return !_path.isEmpty();
    }

    QString   _path;
    quint64   _inode;
    QDateTime _modtime;
    int       _type;
    QByteArray _etag;
    QByteArray _fileId;
    qint64     _fileSize;
    QByteArray _remotePerm;
    int       _mode;
};

bool OWNCLOUDSYNC_EXPORT
operator==(const SyncJournalFileRecord & lhs,
           const SyncJournalFileRecord & rhs);

class SyncJournalErrorBlacklistRecord
{
public:
    SyncJournalErrorBlacklistRecord()
        : _retryCount(0)
        , _lastTryModtime(0)
        , _lastTryTime(0)
        , _ignoreDuration(0)
    {}

    /// The number of times the operation was unsuccessful so far.
    int        _retryCount;

    /// The last error string.
    QString    _errorString;

    time_t     _lastTryModtime;
    QByteArray _lastTryEtag;

    /// The last time the operation was attempted (in s since epoch).
    time_t     _lastTryTime;

    /// The number of seconds the file shall be ignored.
    time_t     _ignoreDuration;

    QString    _file;

    bool isValid() const;

    /** Takes an old blacklist entry and updates it for a new sync result.
     *
     * The old entry may be invalid, then a fresh entry is created.
     * If the returned record is invalid, the file shall not be
     * blacklisted.
     */
    static SyncJournalErrorBlacklistRecord update(
            const SyncJournalErrorBlacklistRecord& old, const SyncFileItem& item);
};

}

#endif // SYNCJOURNALFILERECORD_H
