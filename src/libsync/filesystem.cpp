/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "filesystem.h"

#include "utility.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <qabstractfileengine.h>
#endif

#ifdef Q_OS_WIN
#include <windef.h>
#include <winbase.h>
#endif

// We use some internals of csync:
extern "C" int c_utimes(const char *, const struct timeval *);
extern "C" void csync_win32_set_file_hidden( const char *file, bool h );

extern "C" {
#include "vio/csync_vio_handle.h"
#include "vio/csync_vio_file_stat.h"
#include "vio/csync_vio_local.h"
}

namespace OCC {

bool FileSystem::fileEquals(const QString& fn1, const QString& fn2)
{
    // compare two files with given filename and return true if they have the same content
    QFile f1(fn1);
    QFile f2(fn2);
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
        qDebug() << "fileEquals: Failed to open " << fn1 << "or" << fn2;
        return false;
    }

    if (f1.size() != f2.size()) {
        return false;
    }

    const int BufferSize = 16 * 1024;
    char buffer1[BufferSize];
    char buffer2[BufferSize];
    do {
        int r = f1.read(buffer1, BufferSize);
        if (f2.read(buffer2, BufferSize) != r) {
            // this should normaly not happen: the file are supposed to have the same size.
            return false;
        }
        if (r <= 0) {
            return true;
        }
        if (memcmp(buffer1, buffer2, r) != 0) {
            return false;
        }
    } while (true);
    return false;
}

void FileSystem::setFileHidden(const QString& filename, bool hidden)
{
    return csync_win32_set_file_hidden(filename.toUtf8().constData(), hidden);
}

time_t FileSystem::getModTime(const QString &filename)
{
    csync_vio_file_stat_t* stat = csync_vio_file_stat_new();
    qint64 result = -1;
    if (csync_vio_local_stat(filename.toUtf8().data(), stat) != -1
            && (stat->fields & CSYNC_VIO_FILE_STAT_FIELDS_MTIME))
    {
        result = stat->mtime;
    }
    else
    {
        qDebug() << "Could not get modification time for" << filename
                 << "with csync, using QFileInfo";
        result = Utility::qDateTimeToTime_t(QFileInfo(filename).lastModified());
    }
    csync_vio_file_stat_destroy(stat);
    return result;
}

bool FileSystem::setModTime(const QString& filename, time_t modTime)
{
    struct timeval times[2];
    times[0].tv_sec = times[1].tv_sec = modTime;
    times[0].tv_usec = times[1].tv_usec = 0;
    int rc = c_utimes(filename.toUtf8().data(), times);
    if (rc != 0) {
        qDebug() << "Error setting mtime for" << filename
                 << "failed: rc" << rc << ", errno:" << errno;
        return false;
    }
    return true;
}

bool FileSystem::renameReplace(const QString& originFileName, const QString& destinationFileName, QString* errorString)
{
#ifndef Q_OS_WIN
    bool success;
    QFile orig(originFileName);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    success = orig.fileEngine()->rename(destinationFileName);
    // qDebug() << "Renaming " << tmpFile.fileName() << " to " << fn;
#else
    // We want a rename that also overwite.  QFile::rename does not overwite.
    // Qt 5.1 has QSaveFile::renameOverwrite we cold use.
    // ### FIXME
    QFile::remove(destinationFileName);
    success = orig.rename(destinationFileName);
#endif
    if (!success) {
        *errorString = orig.errorString();
        qDebug() << "FAIL: renaming temp file to final failed: " << *errorString ;
        return false;
    }
#else //Q_OS_WIN
    BOOL ok;
    ok = MoveFileEx((wchar_t*)originFileName.utf16(),
                    (wchar_t*)destinationFileName.utf16(),
                    MOVEFILE_REPLACE_EXISTING+MOVEFILE_COPY_ALLOWED+MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        wchar_t *string = 0;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL, ::GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPWSTR)&string, 0, NULL);

        *errorString = QString::fromWCharArray(string);
        LocalFree((HLOCAL)string);
        return false;
    }
#endif
    return true;
}



}
