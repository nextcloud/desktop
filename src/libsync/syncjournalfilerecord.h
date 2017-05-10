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

    /// Creates a record from an existing item while updating the inode
    SyncJournalFileRecord(const SyncFileItem &, const QString &localFileName);

    /** Creates a basic SyncFileItem from the record
     *
     * This is intended in particular for read-update-write cycles that need
     * to go through a a SyncFileItem, like PollJob.
     */
    SyncFileItem toSyncFileItem();

    bool isValid()
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

    QString _path;
    quint64 _inode;
    QDateTime _modtime;
    int _type;
    QByteArray _etag;
    QByteArray _fileId;
    qint64 _fileSize;
    QByteArray _remotePerm;
    bool _serverHasIgnoredFiles;
    QByteArray _checksumHeader;
};

bool OWNCLOUDSYNC_EXPORT
operator==(const SyncJournalFileRecord &lhs,
    const SyncJournalFileRecord &rhs);

class SyncJournalErrorBlacklistRecord
{
public:
    SyncJournalErrorBlacklistRecord()
        : _retryCount(0)
        , _lastTryModtime(0)
        , _lastTryTime(0)
        , _ignoreDuration(0)
    {
    }

    /// The number of times the operation was unsuccessful so far.
    int _retryCount;

    /// The last error string.
    QString _errorString;

    time_t _lastTryModtime;
    QByteArray _lastTryEtag;

    /// The last time the operation was attempted (in s since epoch).
    time_t _lastTryTime;

    /// The number of seconds the file shall be ignored.
    time_t _ignoreDuration;

    QString _file;
    QString _renameTarget;

    bool isValid() const;
};
}

#endif // SYNCJOURNALFILERECORD_H
