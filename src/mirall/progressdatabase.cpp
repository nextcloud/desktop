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

#include "progressdatabase.h"
#include <QFile>

namespace Mirall {

QDataStream& operator<<(QDataStream&d , const ProgressDatabase::DownloadInfo &i)
{ return d << i.tmpfile << i.etag; }
QDataStream& operator>>(QDataStream&d , ProgressDatabase::DownloadInfo &i)
{ return d >> i.tmpfile >> i.etag;  }
QDataStream& operator<<(QDataStream&d , const ProgressDatabase::UploadInfo &i)
{ return d << i.chunk << i.transferid << i.size << qlonglong(i.mtime); }
QDataStream& operator>>(QDataStream&d , ProgressDatabase::UploadInfo &i) {
    qlonglong mtime;
    d >> i.chunk >> i.transferid >> i.size >> mtime;
    i.mtime = mtime;
    return d;
}


void ProgressDatabase::load(const QString& rootDirectory)
{
    QMutexLocker locker(&_mutex);
    QFile f(rootDirectory + "/.csync-progressdatabase");
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    QDataStream stream(&f);

    QByteArray magic;
    stream >> magic;
    if (magic != "csyncpdb_1")
        return;

    stream >> _down >> _up;
}

void ProgressDatabase::save(const QString& rootDirectory)
{
    QMutexLocker locker(&_mutex);
    QFile f(rootDirectory + "/.csync-progressdatabase");
    if (!f.open(QIODevice::WriteOnly)) {
        return;
    }
    QDataStream stream(&f);
    stream << QByteArray("csyncpdb_1") << _down << _up;
}

}
