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
#include <QCoreApplication>
#include <QDebug>
#include <QCryptographicHash>

#ifdef ZLIB_FOUND
#include <zlib.h>
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <qabstractfileengine.h>
#endif

#ifdef Q_OS_WIN
#include <windef.h>
#include <winbase.h>
#include <fcntl.h>

#endif

// We use some internals of csync:
extern "C" int c_utimes(const char *, const struct timeval *);
extern "C" void csync_win32_set_file_hidden( const char *file, bool h );

extern "C" {
#include "csync.h"
#include "vio/csync_vio_local.h"
#include "std/c_path.h"
}

namespace OCC {

QString FileSystem::longWinPath( const QString& inpath )
{
    QString path(inpath);

#ifdef Q_OS_WIN
    path = QString::fromWCharArray( static_cast<wchar_t*>( c_utf8_path_to_locale(inpath.toUtf8() ) ) );
#endif
    return path;
}

bool FileSystem::fileEquals(const QString& fn1, const QString& fn2)
{
    // compare two files with given filename and return true if they have the same content
    QFile f1(fn1);
    QFile f2(fn2);
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
        qDebug() << "fileEquals: Failed to open " << fn1 << "or" << fn2;
        return false;
    }

    if (getSize(fn1) != getSize(fn2)) {
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
#ifdef _WIN32
    QString fName = longWinPath(filename);
    DWORD dwAttrs;

    dwAttrs = GetFileAttributesW( (wchar_t*)fName.utf16() );

    if (dwAttrs != INVALID_FILE_ATTRIBUTES) {
        if (hidden && !(dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
            SetFileAttributesW((wchar_t*)fName.utf16(), dwAttrs | FILE_ATTRIBUTE_HIDDEN );
        } else if (!hidden && (dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
            SetFileAttributesW((wchar_t*)fName.utf16(), dwAttrs & ~FILE_ATTRIBUTE_HIDDEN );
        }
    }
#endif
}

time_t FileSystem::getModTime(const QString &filename)
{
    csync_vio_file_stat_t* stat = csync_vio_file_stat_new();
    qint64 result = -1;
    if (csync_vio_local_stat(filename.toUtf8().data(), stat) != -1
            && (stat->fields & CSYNC_VIO_FILE_STAT_FIELDS_MTIME)) {
        result = stat->mtime;
    } else {
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

#ifdef Q_OS_WIN
static bool isLnkFile(const QString& filename)
{
    return filename.endsWith(".lnk");
}
#endif

bool FileSystem::rename(const QString &originFileName,
                        const QString &destinationFileName,
                        QString *errorString)
{
    bool success = false;
    QString error;
#ifdef Q_OS_WIN
    QString orig = longWinPath(originFileName);
    QString dest = longWinPath(destinationFileName);

    if (isLnkFile(originFileName) || isLnkFile(destinationFileName)) {
        success = MoveFileEx((wchar_t*)orig.utf16(),
                             (wchar_t*)dest.utf16(),
                             MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
        if (!success) {
            wchar_t *string = 0;
            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                          NULL, ::GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (LPWSTR)&string, 0, NULL);

            error = QString::fromWCharArray(string);
            LocalFree((HLOCAL)string);
        }
    } else
#endif
    {
        QFile orig(originFileName);
        success = orig.rename(destinationFileName);
        if (!success) {
            error = orig.errorString();
        }
    }

    if (!success) {
        qDebug() << "FAIL: renaming file" << originFileName
                 << "to" << destinationFileName
                 << "failed: " << error;
        if (errorString) {
            *errorString = error;
        }
    }
    return success;
}

bool FileSystem::fileChanged(const QString& fileName,
                             qint64 previousSize,
                             time_t previousMtime)
{
    return getSize(fileName) != previousSize
        || getModTime(fileName) != previousMtime;
}

bool FileSystem::verifyFileUnchanged(const QString& fileName,
                                     qint64 previousSize,
                                     time_t previousMtime)
{
    const qint64 actualSize = getSize(fileName);
    const time_t actualMtime = getModTime(fileName);
    if (actualSize != previousSize || actualMtime != previousMtime) {
        qDebug() << "File" << fileName << "has changed:"
                 << "size: " << previousSize << "<->" << actualSize
                 << ", mtime: " << previousMtime << "<->" << actualMtime;
        return false;
    }
    return true;
}

bool FileSystem::renameReplace(const QString& originFileName,
                               const QString& destinationFileName,
                               qint64 destinationSize,
                               time_t destinationMtime,
                               QString* errorString)
{
    if (fileExists(destinationFileName)
            && fileChanged(destinationFileName, destinationSize, destinationMtime)) {
        if (errorString) {
            *errorString = qApp->translate("FileSystem",
                    "The destination file has an unexpected size or modification time");
        }
        return false;
    }

    return uncheckedRenameReplace(originFileName, destinationFileName, errorString);
}

bool FileSystem::uncheckedRenameReplace(const QString& originFileName,
                                        const QString& destinationFileName,
                                        QString* errorString)
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
    success = true;
    bool destExists = fileExists(destinationFileName);
    if( destExists && !QFile::remove(destinationFileName) ) {
        *errorString = orig.errorString();
        qDebug() << Q_FUNC_INFO << "Target file could not be removed.";
        success = false;
    }
    if( success ) {
        success = orig.rename(destinationFileName);
    }
#endif
    if (!success) {
        *errorString = orig.errorString();
        qDebug() << "FAIL: renaming temp file to final failed: " << *errorString ;
        return false;
    }

#else //Q_OS_WIN
    BOOL ok;
    QString orig = longWinPath(originFileName);
    QString dest = longWinPath(destinationFileName);

    ok = MoveFileEx((wchar_t*)orig.utf16(),
                    (wchar_t*)dest.utf16(),
                    MOVEFILE_REPLACE_EXISTING+MOVEFILE_COPY_ALLOWED+MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        wchar_t *string = 0;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL, ::GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPWSTR)&string, 0, NULL);

        *errorString = QString::fromWCharArray(string);
        qDebug() << "FAIL: renaming temp file to final failed: " << *errorString;
        LocalFree((HLOCAL)string);
        return false;
    }
#endif
    return true;
}

bool FileSystem::openAndSeekFileSharedRead(QFile* file, QString* errorOrNull, qint64 seek)
{
    QString errorDummy;
    // avoid many if (errorOrNull) later.
    QString& error = errorOrNull ? *errorOrNull : errorDummy;
    error.clear();

#ifdef Q_OS_WIN
    //
    // The following code is adapted from Qt's QFSFileEnginePrivate::nativeOpen()
    // by including the FILE_SHARE_DELETE share mode.
    //

    // Enable full sharing.
    DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

    int accessRights = GENERIC_READ;
    DWORD creationDisp = OPEN_EXISTING;

    // Create the file handle.
    SECURITY_ATTRIBUTES securityAtts = { sizeof(SECURITY_ATTRIBUTES), NULL, FALSE };
    QString fName = longWinPath(file->fileName());

    HANDLE fileHandle = CreateFileW(
            (const wchar_t*)fName.utf16(),
            accessRights,
            shareMode,
            &securityAtts,
            creationDisp,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

    // Bail out on error.
    if (fileHandle == INVALID_HANDLE_VALUE) {
        error = qt_error_string();
        return false;
    }

    // Convert the HANDLE to an fd and pass it to QFile's foreign-open
    // function. The fd owns the handle, so when QFile later closes
    // the fd the handle will be closed too.
    int fd = _open_osfhandle((intptr_t)fileHandle, _O_RDONLY);
    if (fd == -1) {
        error = "could not make fd from handle";
        return false;
    }
    if (!file->open(fd, QIODevice::ReadOnly, QFile::AutoCloseHandle)) {
        error = file->errorString();
        return false;
    }

    // Seek to the right spot
    LARGE_INTEGER *li = reinterpret_cast<LARGE_INTEGER*>(&seek);
    DWORD newFilePointer = SetFilePointer(fileHandle, li->LowPart, &li->HighPart, FILE_BEGIN);
    if (newFilePointer == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
        error = qt_error_string();
        return false;
    }

    return true;
#else
    if (!file->open(QFile::ReadOnly)) {
        error = file->errorString();
        return false;
    }
    if (!file->seek(seek)) {
        error = file->errorString();
        return false;
    }
    return true;
#endif
}

#ifdef Q_OS_WIN
static qint64 getSizeWithCsync(const QString& filename)
{
    qint64 result = 0;
    csync_vio_file_stat_t* stat = csync_vio_file_stat_new();
    if (csync_vio_local_stat(filename.toUtf8().data(), stat) != -1
            && (stat->fields & CSYNC_VIO_FILE_STAT_FIELDS_SIZE)) {
        result = stat->size;
    } else {
        qDebug() << "Could not get size time for" << filename << "with csync";
    }
    csync_vio_file_stat_destroy(stat);
    return result;
}
#endif

qint64 FileSystem::getSize(const QString& filename)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        // Use csync to get the file size. Qt seems unable to get at it.
        return getSizeWithCsync(filename);
    }
#endif
    return QFileInfo(filename).size();
}

#ifdef Q_OS_WIN
static bool fileExistsWin(const QString& filename)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    QString fName = FileSystem::longWinPath(filename);

    hFind = FindFirstFileW( (wchar_t*)fName.utf16(), &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    FindClose(hFind);
    return true;
}
#endif

bool FileSystem::fileExists(const QString& filename)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        // Use a native check.
        return fileExistsWin(filename);
    }
#endif
    QFileInfo file(filename);
    return file.exists();
}

#ifdef Q_OS_WIN
QString FileSystem::fileSystemForPath(const QString & path)
{
    // See also QStorageInfo (Qt >=5.4) and GetVolumeInformationByHandleW (>= Vista)
    QString drive = path.left(3);
    if (! drive.endsWith(":\\"))
        return QString();

    const size_t fileSystemBufferSize = 4096;
    TCHAR fileSystemBuffer[fileSystemBufferSize];

    if (! GetVolumeInformationW(
            reinterpret_cast<LPCWSTR>(drive.utf16()),
            NULL, 0,
            NULL, NULL, NULL,
            fileSystemBuffer, fileSystemBufferSize)) {
        return QString();
    }
    return QString::fromUtf16(reinterpret_cast<const ushort *>(fileSystemBuffer));
}
#endif

#define BUFSIZE 1024*1024*10

static QByteArray readToCrypto( const QString& filename, QCryptographicHash::Algorithm algo )
{
    const qint64 bufSize = BUFSIZE;
    QByteArray buf(bufSize,0);
    QByteArray arr;
    QCryptographicHash crypto( algo );

    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
        qint64 size;
        while (!file.atEnd()) {
            size = file.read( buf.data(), bufSize );
            if( size > 0 ) {
                crypto.addData(buf.data(), size);
            }
        }
        arr = crypto.result().toHex();
    }
    return arr;
}

QByteArray FileSystem::calcMd5( const QString& filename )
{
    return readToCrypto( filename, QCryptographicHash::Md5 );
}

QByteArray FileSystem::calcSha1( const QString& filename )
{
    return readToCrypto( filename, QCryptographicHash::Sha1 );
}

#ifdef ZLIB_FOUND
QByteArray FileSystem::calcAdler32( const QString& filename )
{
    unsigned int adler = adler32(0L, Z_NULL, 0);
    const qint64 bufSize = BUFSIZE;
    QByteArray buf(bufSize, 0);

    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
        qint64 size;
        while (!file.atEnd()) {
            size = file.read(buf.data(), bufSize);
            if( size > 0 )
                adler = adler32(adler, (const Bytef*) buf.data(), size);
        }
    }

    return QByteArray::number( adler, 16 );
}
#endif

} // namespace OCC
