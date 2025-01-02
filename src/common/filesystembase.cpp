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
#include "utility.h"
#include "common/asserts.h"

#include <QDateTime>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QCoreApplication>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windef.h>
#include <winbase.h>
#include <fcntl.h>
#include <io.h>
#endif

namespace OCC {

Q_LOGGING_CATEGORY(lcFileSystem, "nextcloud.sync.filesystem", QtInfoMsg)

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
    if (filename.isEmpty()) {
        return;
    }
#ifdef _WIN32
    const QString fName = longWinPath(filename);
    DWORD dwAttrs = 0;

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

bool FileSystem::isFileHidden(const QString &filename)
{
#ifdef _WIN32
    if (isLnkFile(filename)) {
        const QString fName = longWinPath(filename);
        DWORD dwAttrs = 0;

        dwAttrs = GetFileAttributesW((wchar_t *)fName.utf16());

        if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        return dwAttrs & FILE_ATTRIBUTE_HIDDEN;
    }
#endif
    return QFileInfo(filename).isHidden();
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
#ifdef  Q_OS_WIN
    if (isLnkFile(filename)) {
        if (!fileExists(filename)) {
            return;
        }
        try {
            const auto permissions = filePermissionsWin(filename);

            std::filesystem::perms allWritePermissions = std::filesystem::perms::_All_write;
            static std::filesystem::perms defaultWritePermissions = std::filesystem::perms::others_write;

            std::filesystem::permissions(filename.toStdString(), allWritePermissions, std::filesystem::perm_options::remove);

            if (!readonly) {
                std::filesystem::permissions(filename.toStdString(), defaultWritePermissions, std::filesystem::perm_options::add);
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            qCWarning(lcFileSystem()) << filename << (readonly ? "readonly" : "read write") << e.what();
        }
        catch (const std::system_error &e)
        {
            qCWarning(lcFileSystem()) << filename << e.what();
        }
        catch (...)
        {
            qCWarning(lcFileSystem()) << filename;
        }
        return;
    }
#endif
    QFile file(filename);
    QFile::Permissions permissions = file.permissions();

    QFile::Permissions allWritePermissions = QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther | QFile::WriteOwner;
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

bool FileSystem::setFileReadOnlyWeak(const QString &filename, bool readonly)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        try {
            const auto permissions = filePermissionsWin(filename);

            if (!readonly && static_cast<bool>((permissions & std::filesystem::perms::owner_write))) {
                return false; // already writable enough
            }

            setFileReadOnly(filename, readonly);
            return true;
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            qCWarning(lcFileSystem()) << filename << (readonly ? "readonly" : "read write") << e.what();
        }
        catch (const std::system_error &e)
        {
            qCWarning(lcFileSystem()) << filename << e.what();
        }
        catch (...)
        {
            qCWarning(lcFileSystem()) << filename;
        }
        return false;
    }
#endif
    QFile file(filename);
    QFile::Permissions permissions = file.permissions();

    if (!readonly && (permissions & QFile::WriteOwner)) {
        return false; // already writable enough
    }

    setFileReadOnly(filename, readonly);
    return true;
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
            error = Utility::formatWinError(GetLastError());
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
    bool success = false;
    QFile orig(originFileName);
    // We want a rename that also overwrites.  QFile::rename does not overwrite.
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
    if (!isWritable(destinationFileName)) {
        setFileReadOnly(destinationFileName, false);
    }

    BOOL ok = 0;
    QString orig = longWinPath(originFileName);
    QString dest = longWinPath(destinationFileName);

    ok = MoveFileEx((wchar_t *)orig.utf16(),
        (wchar_t *)dest.utf16(),
        MOVEFILE_REPLACE_EXISTING + MOVEFILE_COPY_ALLOWED + MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        *errorString = Utility::formatWinError(GetLastError());
        qCWarning(lcFileSystem) << "Renaming temp file to final failed: " << *errorString;
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
    SECURITY_ATTRIBUTES securityAtts = { sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE };
    QString fName = longWinPath(file->fileName());

    HANDLE fileHandle = CreateFileW(
        (const wchar_t *)fName.utf16(),
        accessRights,
        shareMode,
        &securityAtts,
        creationDisp,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

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
        error = QStringLiteral("could not make fd from handle");
        CloseHandle(fileHandle);
        return false;
    }
    if (!file->open(fd, QIODevice::ReadOnly, QFile::AutoCloseHandle)) {
        error = file->errorString();
        _close(fd); // implicitly closes fileHandle
        return false;
    }

    // Seek to the right spot
    auto li = reinterpret_cast<LARGE_INTEGER *>(&seek);
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
std::filesystem::perms FileSystem::filePermissionsWin(const QString &filename)
{
    return std::filesystem::status(filename.toStdString()).permissions();
}

void FileSystem::setFilePermissionsWin(const QString &filename, const std::filesystem::perms &perms)
{
    if (!fileExists(filename)) {
        return;
    }
    std::filesystem::permissions(filename.toStdString(), perms);
}

static bool fileExistsWin(const QString &filename)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind = nullptr;
    const QString fName = FileSystem::longWinPath(filename);

    hFind = FindFirstFileW((wchar_t *)fName.utf16(), &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    FindClose(hFind);
    return true;
}

static bool isDirWin(const QString &filename)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind = nullptr;
    const QString fName = FileSystem::longWinPath(filename);

    hFind = FindFirstFileW((wchar_t *)fName.utf16(), &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    FindClose(hFind);
    return FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
}

static bool isFileWin(const QString &filename)
{
    return !isDirWin(filename);
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

bool FileSystem::isDir(const QString &filename, const QFileInfo &fileInfo)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        // Use a native check.
        return isDirWin(filename);
    }
#endif
    bool re = fileInfo.isDir();
    // if the filename is different from the filename in fileInfo, the fileInfo is
    // not valid. There needs to be one initialised here. Otherwise the incoming
    // fileInfo is re-used.
    if (fileInfo.filePath() != filename) {
        QFileInfo myFI(filename);
        re = myFI.isDir();
    }
    return re;
}

bool FileSystem::isFile(const QString &filename, const QFileInfo &fileInfo)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        // Use a native check.
        return isFileWin(filename);
    }
#endif
    bool re = fileInfo.isDir();
    // if the filename is different from the filename in fileInfo, the fileInfo is
    // not valid. There needs to be one initialised here. Otherwise the incoming
    // fileInfo is re-used.
    if (fileInfo.filePath() != filename) {
        QFileInfo myFI(filename);
        re = myFI.isFile();
    }
    return re;
}

bool FileSystem::isWritable(const QString &filename, const QFileInfo &fileInfo)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        try {
            const auto permissions = filePermissionsWin(filename);
            return static_cast<bool>((permissions & std::filesystem::perms::owner_write));
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            qCWarning(lcFileSystem()) << filename << e.what();
        }
        catch (const std::system_error &e)
        {
            qCWarning(lcFileSystem()) << filename << e.what();
        }
        catch (...)
        {
            qCWarning(lcFileSystem()) << filename;
        }
        return false;
    }
#endif
    bool re = fileInfo.isWritable();
    // if the filename is different from the filename in fileInfo, the fileInfo is
    // not valid. There needs to be one initialised here. Otherwise the incoming
    // fileInfo is re-used.
    if (fileInfo.filePath() != filename) {
        QFileInfo myFI(filename);
        re = myFI.isWritable();
    }
    return re;
}

bool FileSystem::isReadable(const QString &filename, const QFileInfo &fileInfo)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        try {
            const auto permissions = filePermissionsWin(filename);
            return static_cast<bool>((permissions & std::filesystem::perms::owner_read));
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            qCWarning(lcFileSystem()) << filename << e.what();
        }
        catch (const std::system_error &e)
        {
            qCWarning(lcFileSystem()) << filename << e.what();
        }
        catch (...)
        {
            qCWarning(lcFileSystem()) << filename;
        }
        return false;
    }
#endif
    bool re = fileInfo.isReadable();
    // if the filename is different from the filename in fileInfo, the fileInfo is
    // not valid. There needs to be one initialised here. Otherwise the incoming
    // fileInfo is re-used.
    if (fileInfo.filePath() != filename) {
        QFileInfo myFI(filename);
        re = myFI.isReadable();
    }
    return re;
}

bool FileSystem::isSymLink(const QString &filename, const QFileInfo &fileInfo)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        return isJunction(filename);
    }
#endif
    bool re = fileInfo.isSymLink();
    // if the filename is different from the filename in fileInfo, the fileInfo is
    // not valid. There needs to be one initialised here. Otherwise the incoming
    // fileInfo is re-used.
    if (fileInfo.filePath() != filename) {
        QFileInfo myFI(filename);
        re = myFI.isSymLink();
    }
    return re;
}

#ifdef Q_OS_WIN
QString FileSystem::fileSystemForPath(const QString &path)
{
    // See also QStorageInfo (Qt >=5.4) and GetVolumeInformationByHandleW (>= Vista)
    QString drive = path.left(2);
    if (!drive.endsWith(QLatin1Char(':')))
        return QString();
    drive.append(QLatin1Char('\\'));

    const size_t fileSystemBufferSize = 4096;
    TCHAR fileSystemBuffer[fileSystemBufferSize];

    if (!GetVolumeInformationW(
            reinterpret_cast<LPCWSTR>(drive.utf16()),
            nullptr, 0,
            nullptr, nullptr, nullptr,
            fileSystemBuffer, fileSystemBufferSize)) {
        return QString();
    }
    return QString::fromUtf16(reinterpret_cast<const ushort *>(fileSystemBuffer));
}
#endif

bool FileSystem::remove(const QString &fileName, QString *errorString)
{
#ifdef Q_OS_WIN
    // You cannot delete a read-only file on windows, but we want to
    // allow that.
    if (!isWritable(fileName)) {
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

bool FileSystem::moveToTrash(const QString &fileName, QString *errorString)
{
    QFile f(fileName);
    if (!f.moveToTrash()) {
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
    // Check if file exists
    const QString fName = longWinPath(fileName);
    DWORD attr = GetFileAttributesW(reinterpret_cast<const wchar_t *>(fName.utf16()));
    if (attr != INVALID_FILE_ATTRIBUTES) {
        // Try to open the file with as much access as possible..
        HANDLE win_h = CreateFileW(
            reinterpret_cast<const wchar_t *>(fName.utf16()),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
            nullptr);

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
    return filename.endsWith(QLatin1String(".lnk"));
}

bool FileSystem::isExcludeFile(const QString &filename)
{
    return filename.compare(QStringLiteral(".sync-exclude.lst"), Qt::CaseInsensitive) == 0
        || filename.compare(QStringLiteral("exclude.lst"), Qt::CaseInsensitive) == 0
        || filename.endsWith(QStringLiteral("/.sync-exclude.lst"), Qt::CaseInsensitive)
        || filename.endsWith(QStringLiteral("/exclude.lst"), Qt::CaseInsensitive);
}

bool FileSystem::isJunction(const QString &filename)
{
#ifdef Q_OS_WIN
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFileEx(reinterpret_cast<const wchar_t *>(longWinPath(filename).utf16()), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, 0);
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

#ifdef Q_OS_WIN
QString FileSystem::pathtoUNC(const QString &_str)
{
    Q_ASSERT(QFileInfo(_str).isAbsolute());
    if (_str.isEmpty()) {
        return _str;
    }
    const QString str = QDir::toNativeSeparators(_str);
    const QLatin1Char sep('\\');

    // we already have a unc path
    if (str.startsWith(sep + sep)) {
        return str;
    }
    // prepend \\?\ and to support long names

    if (str.at(0) == sep) {
        // should not happen as we require the path to be absolute
        return QStringLiteral(R"(\\?)") + str;
    }
    return QStringLiteral(R"(\\?\)") + str;
}
#endif

} // namespace OCC
