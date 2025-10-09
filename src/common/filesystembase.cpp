/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
#include <securitybaseapi.h>
#include <sddl.h>
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
    if (!fileExists(filename)) {
        Q_ASSERT(false);
        return;
    }

    const auto windowsFilename = QDir::toNativeSeparators(filename);
    const auto fileAttributes = GetFileAttributesW(windowsFilename.toStdWString().c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        const auto lastError = GetLastError();
        auto errorMessage = static_cast<char*>(nullptr);
        if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                          nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage, 0, nullptr) == 0) {
            qCWarning(lcFileSystem()) << "GetFileAttributesW" << windowsFilename << (readonly ? "readonly" : "read write") << errorMessage;
        } else {
            qCWarning(lcFileSystem()) << "GetFileAttributesW" << windowsFilename << (readonly ? "readonly" : "read write") << "unknown error" << lastError;
        }
        return;
    }

    auto newFileAttributes = fileAttributes;
    if (readonly) {
        newFileAttributes = newFileAttributes | FILE_ATTRIBUTE_READONLY;
    } else {
        newFileAttributes = newFileAttributes & (~FILE_ATTRIBUTE_READONLY);
    }

    if (SetFileAttributesW(windowsFilename.toStdWString().c_str(), newFileAttributes) == 0) {
        const auto lastError = GetLastError();
        auto errorMessage = static_cast<char*>(nullptr);
        if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage, 0, nullptr) == 0) {
            qCWarning(lcFileSystem()) << "SetFileAttributesW" << windowsFilename << (readonly ? "readonly" : "read write") << errorMessage;
        } else {
            qCWarning(lcFileSystem()) << "SetFileAttributesW" << windowsFilename << (readonly ? "readonly" : "read write") << "unknown error" << lastError;
        }
    }

    if (!readonly) {
        // current read-only folder ACL needs to be removed from files also when making a folder read-write
        // we currently have a too limited set of authorization for files when applying the restrictive ACL for folders on the child files
        setAclPermission(filename, FileSystem::FolderPermissions::ReadWrite, false);
    }

    return;
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
#ifdef Q_OS_MACOS
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

QString FileSystem::joinPath(const QString& path, const QString& file)
{
    if (path.isEmpty()) {
        qCWarning(lcFileSystem).nospace() << "joinPath called with an empty path; returning file=" << file;
        return QDir::toNativeSeparators(file);
    }

    if (file.isEmpty()) {
        qCWarning(lcFileSystem).nospace() << "joinPath called with an empty file; returning path=" << path;
        return QDir::toNativeSeparators(path);
    }

    if (const auto lastChar = path[path.size() - 1]; lastChar == QLatin1Char{'/'} || lastChar == QLatin1Char{'\\'}) {
        return QDir::toNativeSeparators(path + file);
    }

    return QDir::toNativeSeparators(path + QDir::separator() + file);
}

#ifdef Q_OS_WIN
std::filesystem::perms FileSystem::filePermissionsWinSymlinkSafe(const QString &filename)
{
    try {
        return std::filesystem::symlink_status(filename.toStdWString()).permissions();
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when checking permissions of symlink" << e.what() << "- path1:" << e.path1().c_str() << "- path2:" << e.path2().c_str();
    }
    catch (const std::system_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when checking permissions of symlink" << e.what() << "- path:" << filename;
    }
    catch (...)
    {
        qCWarning(lcFileSystem()) << "exception when checking permissions of symlink -  path:" << filename;
    }

    return {};
}

std::filesystem::perms FileSystem::filePermissionsWin(const QString &filename)
{
    try {
        return std::filesystem::status(filename.toStdWString()).permissions();
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when checking permissions of symlink" << e.what() << "- path1:" << e.path1().c_str() << "- path2:" << e.path2().c_str();
    }
    catch (const std::system_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when checking permissions of symlink" << e.what() << "- path:" << filename;
    }
    catch (...)
    {
        qCWarning(lcFileSystem()) << "exception when checking permissions of symlink -  path:" << filename;
    }

    return {};
}

void FileSystem::setFilePermissionsWin(const QString &filename, const std::filesystem::perms &perms)
{
    if (!fileExists(filename)) {
        return;
    }

    try {
        std::filesystem::permissions(filename.toStdWString(), perms);
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when checking permissions of symlink" << e.what() << "- path1:" << e.path1().c_str() << "- path2:" << e.path2().c_str();
    }
    catch (const std::system_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when checking permissions of symlink" << e.what() << "- path:" << filename;
    }
    catch (...)
    {
        qCWarning(lcFileSystem()) << "exception when checking permissions of symlink -  path:" << filename;
    }
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
        re = QFileInfo::exists(filename);
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
    const auto &windowsSafeFileName = FileSystem::longWinPath(fileName);
#ifdef Q_OS_WIN
    // You cannot delete a read-only file on windows, but we want to
    // allow that.
    setFileReadOnly(windowsSafeFileName, false);
#endif
    const auto deletedFileInfo = QFileInfo{windowsSafeFileName};
    if (!deletedFileInfo.exists()) {
        qCWarning(lcFileSystem()) << windowsSafeFileName << "has been already deleted";
    }

    QFile f(windowsSafeFileName);
    if (!f.remove()) {
        if (errorString) {
            *errorString = f.errorString();
        }
        qCWarning(lcFileSystem()) << f.errorString() << windowsSafeFileName;

#if defined Q_OS_WIN
        try {
            const auto permissionsDisplayHelper = [] (std::filesystem::perms currentPermissions) {
                const auto unitaryHelper = [currentPermissions] (std::filesystem::perms testedPermission, char permissionChar) {
                    return (static_cast<bool>(currentPermissions & testedPermission) ? permissionChar : '-');
                };

                qCInfo(lcFileSystem()) << unitaryHelper(std::filesystem::perms::owner_read, 'r')
                                       << unitaryHelper(std::filesystem::perms::owner_write, 'w')
                                       << unitaryHelper(std::filesystem::perms::owner_exec, 'x')
                                       << unitaryHelper(std::filesystem::perms::group_read, 'r')
                                       << unitaryHelper(std::filesystem::perms::group_write, 'w')
                                       << unitaryHelper(std::filesystem::perms::group_exec, 'x')
                                       << unitaryHelper(std::filesystem::perms::others_read, 'r')
                                       << unitaryHelper(std::filesystem::perms::others_write, 'w')
                                       << unitaryHelper(std::filesystem::perms::others_exec, 'x');
            };

            const auto unsafeFilePermissions = filePermissionsWin(windowsSafeFileName);
            permissionsDisplayHelper(unsafeFilePermissions);

            const auto safeFilePermissions = filePermissionsWinSymlinkSafe(windowsSafeFileName);
            permissionsDisplayHelper(safeFilePermissions);
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            qCWarning(lcFileSystem()) << "exception when modifying permissions" << e.what() << "- path1:" << e.path1().c_str() << "- path2:" << e.path2().c_str();
        }
        catch (const std::system_error &e)
        {
            qCWarning(lcFileSystem()) << "exception when modifying permissions" << e.what() << "- path:" << windowsSafeFileName;
        }
        catch (...)
        {
            qCWarning(lcFileSystem()) << "exception when modifying permissions -  path:" << windowsSafeFileName;
        }
#endif

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

bool FileSystem::setAclPermission(const QString &unsafePath, FolderPermissions permissions, bool applyAlsoToFiles)
{
    SECURITY_INFORMATION info = DACL_SECURITY_INFORMATION;
    std::unique_ptr<char[]> securityDescriptor;
    auto neededLength = 0ul;

    const auto path = longWinPath(unsafePath);

    const auto safePathFileInfo = QFileInfo{path};

    if (!GetFileSecurityW(path.toStdWString().c_str(), info, nullptr, 0, &neededLength)) {
        const auto lastError = GetLastError();
        if (lastError != ERROR_INSUFFICIENT_BUFFER) {
            qCWarning(lcFileSystem) << "error when calling GetFileSecurityW" << path << Utility::formatWinError(lastError);
            return false;
        }

        securityDescriptor.reset(new char[neededLength]);

        if (!GetFileSecurityW(path.toStdWString().c_str(), info, securityDescriptor.get(), neededLength, &neededLength)) {
            qCWarning(lcFileSystem) << "error when calling GetFileSecurityW" << path << Utility::formatWinError(GetLastError());
            return false;
        }
    }

    int daclPresent = false, daclDefault = false;
    PACL resultDacl = nullptr;
    if (!GetSecurityDescriptorDacl(securityDescriptor.get(), &daclPresent, &resultDacl, &daclDefault)) {
        qCWarning(lcFileSystem) << "error when calling GetSecurityDescriptorDacl" << path << Utility::formatWinError(GetLastError());
        return false;
    }
    if (!daclPresent || !resultDacl) {
        qCWarning(lcFileSystem) << "error when calling DACL needed to set a folder read-only or read-write is missing" << path;
        return false;
    }

    PSID sid = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-5-32-545", &sid))
    {
        qCWarning(lcFileSystem) << "error when calling ConvertStringSidToSidA" << path << Utility::formatWinError(GetLastError());
        return false;
    }

    ACL_SIZE_INFORMATION aclSize;
    if (!GetAclInformation(resultDacl, &aclSize, sizeof(aclSize), AclSizeInformation)) {
        qCWarning(lcFileSystem) << "error when calling GetAclInformation" << path << Utility::formatWinError(GetLastError());
        return false;
    }

    const auto newAclSize = aclSize.AclBytesInUse + sizeof(ACCESS_DENIED_ACE) + GetLengthSid(sid);
    qCDebug(lcFileSystem) << "allocated a new DACL object of size" << newAclSize;

    std::unique_ptr<ACL> newDacl{reinterpret_cast<PACL>(new char[newAclSize])};
    if (!InitializeAcl(newDacl.get(), newAclSize, ACL_REVISION)) {
        const auto lastError = GetLastError();
        if (lastError == ERROR_INSUFFICIENT_BUFFER) {
            qCWarning(lcFileSystem) << "insufficient memory error when calling InitializeAcl" << path;
            return false;
        }

        qCWarning(lcFileSystem) << "error when calling InitializeAcl" << path << Utility::formatWinError(lastError);
        return false;
    }

    if (permissions == FileSystem::FolderPermissions::ReadOnly) {
        if (!AddAccessDeniedAceEx(newDacl.get(), ACL_REVISION, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE,
                                  FILE_DELETE_CHILD | DELETE | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_APPEND_DATA, sid)) {
            qCWarning(lcFileSystem) << "error when calling AddAccessDeniedAce" << path << Utility::formatWinError(GetLastError());
            return false;
        }
    }

    for (int i = 0; i < aclSize.AceCount; ++i) {
        void *currentAce = nullptr;
        if (!GetAce(resultDacl, i, &currentAce)) {
            qCWarning(lcFileSystem) << "error when calling GetAce" << path << Utility::formatWinError(GetLastError());
            return false;
        }

        const auto currentAceHeader = reinterpret_cast<PACE_HEADER>(currentAce);

        if (permissions == FileSystem::FolderPermissions::ReadWrite && (ACCESS_DENIED_ACE_TYPE == (currentAceHeader->AceType & ACCESS_DENIED_ACE_TYPE))) {
            qCWarning(lcFileSystem) << "AceHeader" << path << currentAceHeader->AceFlags << currentAceHeader->AceSize << currentAceHeader->AceType;
            continue;
        }

        if (!AddAce(newDacl.get(), ACL_REVISION, i + 1, currentAce, currentAceHeader->AceSize)) {
            const auto lastError = GetLastError();
            if (lastError == ERROR_INSUFFICIENT_BUFFER) {
                qCWarning(lcFileSystem) << "insufficient memory error when calling AddAce" << path;
                return false;
            }

            if (lastError == ERROR_INVALID_PARAMETER) {
                qCWarning(lcFileSystem) << "invalid parameter error when calling AddAce" << path << "ACL size" << newAclSize;
                return false;
            }

            qCWarning(lcFileSystem) << "error when calling AddAce" << path << Utility::formatWinError(lastError) << "acl index" << (i + 1);
            return false;
        }
    }

    SECURITY_DESCRIPTOR newSecurityDescriptor;
    if (!InitializeSecurityDescriptor(&newSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION)) {
        qCWarning(lcFileSystem) << "error when calling InitializeSecurityDescriptor" << path << Utility::formatWinError(GetLastError());
        return false;
    }

    if (!SetSecurityDescriptorDacl(&newSecurityDescriptor, true, newDacl.get(), false)) {
        qCWarning(lcFileSystem) << "error when calling SetSecurityDescriptorDacl" << path << Utility::formatWinError(GetLastError());
        return false;
    }

    if (safePathFileInfo.isDir() && applyAlsoToFiles) {
        const auto currentFolder = safePathFileInfo.dir();
        const auto childFiles = currentFolder.entryList(QDir::Filter::Files);
        for (const auto &oneEntry : childFiles) {
            const auto childFile = joinPath(path, oneEntry);

            const auto &childFileStdWString = childFile.toStdWString();
            const auto attributes = GetFileAttributes(childFileStdWString.c_str());

                   // testing if that could be a pure virtual placeholder file (i.e. CfApi file without data)
                   // we do not want to trigger implicit hydration ourself
            if ((attributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0) {
                continue;
            }

            if (!SetFileSecurityW(childFileStdWString.c_str(), info, &newSecurityDescriptor)) {
                qCWarning(lcFileSystem) << "error when calling SetFileSecurityW" << childFile << Utility::formatWinError(GetLastError());
                return false;
            }
        }
    }

    if (!SetFileSecurityW(QDir::toNativeSeparators(path).toStdWString().c_str(), info, &newSecurityDescriptor)) {
        qCWarning(lcFileSystem) << "error when calling SetFileSecurityW" << QDir::toNativeSeparators(path) << Utility::formatWinError(GetLastError());
        return false;
    }

    return true;
}

#endif

} // namespace OCC
