/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#ifndef PROGRESSDATABASE_H
#define PROGRESSDATABASE_H

#include <QString>
#include <QList>
#include <QHash>
#include <QMutex>


namespace Mirall {

class ProgressDatabase {
public:
    struct DownloadInfo {
        QString tmpfile;
        QByteArray etag;
    };
    typedef QHash<QString, DownloadInfo > DownloadInfoHash;
    struct UploadInfo {
        int chunk;
        int transferid;
        quint64 size; //currently unused
        time_t mtime;
    };
    typedef QHash<QString, UploadInfo > UploadInfoHash;

    void load(const QString &rootDirectory);
    void save(const QString &rootDirectory);

    const DownloadInfo *getDownloadInfo(const QString &file) const {
        QMutexLocker locker(&_mutex);
        DownloadInfoHash::const_iterator it = _down.constFind(file);
        if (it == _down.end())
            return 0;
        return &it.value();
    }

    const UploadInfo *getUploadInfo(const QString &file) const {
        QMutexLocker locker(&_mutex);
        UploadInfoHash::const_iterator it = _up.constFind(file);
        if (it == _up.end())
            return 0;
        return &it.value();
    }

    void setDownloadInfo(const QString &file, const DownloadInfo &i) {
        QMutexLocker locker(&_mutex);
        _down[file] = i;
    }

    void setUploadInfo(const QString &file, const UploadInfo &i) {
        QMutexLocker locker(&_mutex);
        _up[file] = i;
    }

    void remove(const QString &file) {
        QMutexLocker locker(&_mutex);
        _down.remove(file);
        _up.remove(file);
    }

private:
    DownloadInfoHash _down;
    UploadInfoHash _up;
    QString _rootPath;
    mutable QMutex _mutex;
};


}

#endif
