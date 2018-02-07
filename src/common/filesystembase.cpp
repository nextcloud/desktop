/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "filesystembase.h"

#include <QDateTime>
#include <QFile>
#include <QCryptographicHash>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef ZLIB_FOUND
#include <zlib.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <windef.h>
#include <winbase.h>
#include <fcntl.h>
#include <io.h>
#endif

namespace OCC {

Q_LOGGING_CATEGORY(lcFileSystem, "sync.filesystem", QtInfoMsg)

QString FileSystem::longWinPath(const QString &inpath)
{
#ifdef Q_OS_WIN
    return pathtoUNC(inpath);
#else
    return inpath;
#endif
}

void FileSystem::setFileHidden(const QString &filename, bool hidden)
{
#ifdef _WIN32
    QString fName = longWinPath(filename);
    DWORD dwAttrs;

    dwAttrs = GetFileAttributesW((wchar_t *)fName.utf16());

    if (dwAttrs != INVALID_FILE_ATTRIBUTES) {
        if (hidden && !(dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
            SetFileAttributesW((wchar_t *)fName.utf16(), dwAttrs | FILE_ATTRIBUTE_HIDDEN);
        } else if (!hidden && (dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
            SetFileAttributesW((wchar_t *)fName.utf16(), dwAttrs & ~FILE_ATTRIBUTE_HIDDEN);
        }
    }
#else
    Q_UNUSED(filename);
    Q_UNUSED(hidden);
#endif
}

static QFile::Permissions getDefaultWritePermissions()
{
    QFile::Permissions result = QFile::WriteUser;
#ifndef Q_OS_WIN
    mode_t mask = umask(0);
    umask(mask);
    if (!(mask & S_IWGRP)) {
        result |= QFile::WriteGroup;
    }
    if (!(mask & S_IWOTH)) {
        result |= QFile::WriteOther;
    }
#endif
    return result;
}

void FileSystem::setFileReadOnly(const QString &filename, bool readonly)
{
    QFile file(filename);
    QFile::Permissions permissions = file.permissions();

    QFile::Permissions allWritePermissions =
        QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther | QFile::WriteOwner;
    static QFile::Permissions defaultWritePermissions = getDefaultWritePermissions();

    permissions &= ~allWritePermissions;
    if (!readonly) {
        permissions |= defaultWritePermissions;
    }
    file.setPermissions(permissions);
}

void FileSystem::setFolderMinimumPermissions(const QString &filename)
{
#ifdef Q_OS_MAC
    QFile::Permissions perm = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    QFile file(filename);
    file.setPermissions(perm);
#else
    Q_UNUSED(filename);
#endif
}


void FileSystem::setFileReadOnlyWeak(const QString &filename, bool readonly)
{
    QFile file(filename);
    QFile::Permissions permissions = file.permissions();

    if (!readonly && (permissions & QFile::WriteOwner)) {
        return; // already writable enough
    }

    setFileReadOnly(filename, readonly);
}

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
        success = MoveFileEx((wchar_t *)orig.utf16(),
            (wchar_t *)dest.utf16(),
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
        qCWarning(lcFileSystem) << "Error renaming file" << originFileName
                                << "to" << destinationFileName
                                << "failed: " << error;
        if (errorString) {
            *errorString = error;
        }
    }
    return success;
}

bool FileSystem::uncheckedRenameReplace(const QString &originFileName,
    const QString &destinationFileName,
    QString *errorString)
{
#ifndef Q_OS_WIN
    bool success;
    QFile orig(originFileName);
    // We want a rename that also overwites.  QFile::rename does not overwite.
    // Qt 5.1 has QSaveFile::renameOverwrite we could use.
    // ### FIXME
    success = true;
    bool destExists = fileExists(destinationFileName);
    if (destExists && !QFile::remove(destinationFileName)) {
        *errorString = orig.errorString();
        qCWarning(lcFileSystem) << "Target file could not be removed.";
        success = false;
    }
    if (success) {
        success = orig.rename(destinationFileName);
    }
    if (!success) {
        *errorString = orig.errorString();
        qCWarning(lcFileSystem) << "Renaming temp file to final failed: " << *errorString;
        return false;
    }

#else //Q_OS_WIN
    // You can not overwrite a read-only file on windows.
    if (!QFileInfo(destinationFileName).isWritable()) {
        setFileReadOnly(destinationFileName, false);
    }

    BOOL ok;
    QString orig = longWinPath(originFileName);
    QString dest = longWinPath(destinationFileName);

    ok = MoveFileEx((wchar_t *)orig.utf16(),
        (wchar_t *)dest.utf16(),
        MOVEFILE_REPLACE_EXISTING + MOVEFILE_COPY_ALLOWED + MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        wchar_t *string = 0;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, ::GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&string, 0, NULL);

        *errorString = QString::fromWCharArray(string);
        qCWarning(lcFileSystem) << "Renaming temp file to final failed: " << *errorString;
        LocalFree((HLOCAL)string);
        return false;
    }
#endif
    return true;
}

bool FileSystem::openAndSeekFileSharedRead(QFile *file, QString *errorOrNull, qint64 seek)
{
    QString errorDummy;
    // avoid many if (errorOrNull) later.
    QString &error = errorOrNull ? *errorOrNull : errorDummy;
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
        (const wchar_t *)fName.utf16(),
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
    LARGE_INTEGER *li = reinterpret_cast<LARGE_INTEGER *>(&seek);
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
static bool fileExistsWin(const QString &filename)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    QString fName = FileSystem::longWinPath(filename);

    hFind = FindFirstFileW((wchar_t *)fName.utf16(), &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    FindClose(hFind);
    return true;
}
#endif

bool FileSystem::fileExists(const QString &filename, const QFileInfo &fileInfo)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        // Use a native check.
        return fileExistsWin(filename);
    }
#endif
    bool re = fileInfo.exists();
    // if the filename is different from the filename in fileInfo, the fileInfo is
    // not valid. There needs to be one initialised here. Otherwise the incoming
    // fileInfo is re-used.
    if (fileInfo.filePath() != filename) {
        QFileInfo myFI(filename);
        re = myFI.exists();
    }
    return re;
}

#ifdef Q_OS_WIN
QString FileSystem::fileSystemForPath(const QString &path)
{
    // See also QStorageInfo (Qt >=5.4) and GetVolumeInformationByHandleW (>= Vista)
    QString drive = path.left(2);
    if (!drive.endsWith(":"))
        return QString();
    drive.append('\\');

    const size_t fileSystemBufferSize = 4096;
    TCHAR fileSystemBuffer[fileSystemBufferSize];

    if (!GetVolumeInformationW(
            reinterpret_cast<LPCWSTR>(drive.utf16()),
            NULL, 0,
            NULL, NULL, NULL,
            fileSystemBuffer, fileSystemBufferSize)) {
        return QString();
    }
    return QString::fromUtf16(reinterpret_cast<const ushort *>(fileSystemBuffer));
}
#endif

#define BUFSIZE qint64(500 * 1024) // 500 KiB

static QByteArray readToCrypto( const QString& filename, QCryptographicHash::Algorithm algo )
 {
     QFile file(filename);
     QByteArray arr;
     QCryptographicHash crypto( algo );

     if (file.open(QIODevice::ReadOnly)) {
         if (crypto.addData(&file)) {
             arr = crypto.result().toHex();
         }
     }
     return arr;
 }

QByteArray FileSystem::calcMd5(const QString &filename)
{
    return readToCrypto(filename, QCryptographicHash::Md5);
}

QByteArray FileSystem::calcSha1(const QString &filename)
{
    return readToCrypto(filename, QCryptographicHash::Sha1);
}

#ifdef ZLIB_FOUND
QByteArray FileSystem::calcAdler32(const QString &filename)
{
    QFile file(filename);
    const qint64 bufSize = qMin(BUFSIZE, file.size() + 1);
    QByteArray buf(bufSize, Qt::Uninitialized);

    unsigned int adler = adler32(0L, Z_NULL, 0);
    if (file.open(QIODevice::ReadOnly)) {
        qint64 size;
        while (!file.atEnd()) {
            size = file.read(buf.data(), bufSize);
            if (size > 0)
                adler = adler32(adler, (const Bytef *)buf.data(), size);
        }
    }

    return QByteArray::number(adler, 16);
}
#endif

QString FileSystem::makeConflictFileName(const QString &fn, const QDateTime &dt)
{
    QString conflictFileName(fn);
    // Add _conflict-XXXX  before the extension.
    int dotLocation = conflictFileName.lastIndexOf('.');
    // If no extension, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
    if (dotLocation <= conflictFileName.lastIndexOf('/') + 1) {
        dotLocation = conflictFileName.size();
    }
    QString timeString = dt.toString("yyyyMMdd-hhmmss");

    // Additional marker
    QByteArray conflictFileUserName = qgetenv("CSYNC_CONFLICT_FILE_USERNAME");
    if (conflictFileUserName.isEmpty())
        conflictFileName.insert(dotLocation, "_conflict-" + timeString);
    else
        conflictFileName.insert(dotLocation, "_conflict_" + QString::fromUtf8(conflictFileUserName) + "-" + timeString);

    return conflictFileName;
}

bool FileSystem::remove(const QString &fileName, QString *errorString)
{
#ifdef Q_OS_WIN
    // You cannot delete a read-only file on windows, but we want to
    // allow that.
    if (!QFileInfo(fileName).isWritable()) {
        setFileReadOnly(fileName, false);
    }
#endif
    QFile f(fileName);
    if (!f.remove()) {
        if (errorString) {
            *errorString = f.errorString();
        }
        return false;
    }
    return true;
}

bool FileSystem::isFileLocked(const QString &fileName)
{
#ifdef Q_OS_WIN
    const wchar_t *wuri = reinterpret_cast<const wchar_t *>(fileName.utf16());
    // Check if file exists
    DWORD attr = GetFileAttributesW(wuri);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        // Try to open the file with as much access as possible..
        HANDLE win_h = CreateFileW(
            wuri,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
            NULL);

        if (win_h == INVALID_HANDLE_VALUE) {
            /* could not be opened, so locked? */
            /* 32 == ERROR_SHARING_VIOLATION */
            return true;
        } else {
            CloseHandle(win_h);
        }
    }
#else
    Q_UNUSED(fileName);
#endif
    return false;
}

bool FileSystem::isLnkFile(const QString &filename)
{
    return filename.endsWith(".lnk");
}

bool FileSystem::isJunction(const QString &filename)
{
#ifdef Q_OS_WIN
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFileEx((const wchar_t *)filename.utf16(), FindExInfoBasic, &findData, FindExSearchNameMatch, NULL, 0);
    if (hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);
        return false;
    }
    return findData.dwFileAttributes != INVALID_FILE_ATTRIBUTES
        && findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT
        && findData.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT;
#else
    Q_UNUSED(filename);
    return false;
#endif
}

} // namespace OCC
