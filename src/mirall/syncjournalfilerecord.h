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

namespace Mirall {

class SyncFileItem;

class SyncJournalFileRecord
{
public:
    SyncJournalFileRecord();
    SyncJournalFileRecord(const SyncFileItem&, const QString &localFileName);

    bool isValid() {
        return !_path.isEmpty();
    }

    // query("SELECT path, inode, uid, gid, mode, modtime, type, md5 FROM metadata WHERE phash=:phash");

    QString   _path;
    quint64   _inode;
    QDateTime _modtime;
    int       _type;
    QString   _etag;
    QString   _fileId;
    int       _uid;
    int       _gid;
    int       _mode;
};

class SyncJournalBlacklistRecord
{
public:
    SyncJournalBlacklistRecord() : _retryCount(0), _lastTryModtime(0) { }

    SyncJournalBlacklistRecord(const SyncFileItem&, int retries);

    // query("SELECT path, inode, uid, gid, mode, modtime, type, md5 FROM metadata WHERE phash=:phash");
    int        _retryCount;
    QString    _errorString;
    time_t     _lastTryModtime;
    QByteArray _lastTryEtag;
    QString    _file;

    bool isValid() { return(_lastTryEtag.length() > 0 || _lastTryModtime > 0); }
};

}

#endif // SYNCJOURNALFILERECORD_H
