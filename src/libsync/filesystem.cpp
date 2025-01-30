/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "filesystem.h"

#include "common/utility.h"
#include "csync.h"
#include "vio/csync_vio_local.h"
#include "std/c_time.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>

#include <array>
#include <string_view>

#ifdef Q_OS_WIN
#include <securitybaseapi.h>
#include <sddl.h>
#endif

namespace
{
constexpr std::array<const char *, 2> lockFilePatterns = {{".~lock.", "~$"}};
constexpr std::array<std::string_view, 8> officeFileExtensions = {"doc", "docx", "xls", "xlsx", "ppt", "pptx", "odt", "odp"};
// iterates through the dirPath to find the matching fileName
QString findMatchingUnlockedFileInDir(const QString &dirPath, const QString &lockFileName)
{
    QString foundFilePath;
    const QDir dir(dirPath);
    const auto entryList = dir.entryInfoList(QDir::Files);
    for (const auto &candidateUnlockedFileInfo : entryList) {
        const auto candidateUnlockFileName = candidateUnlockedFileInfo.fileName();
        const auto lockFilePatternFoundIt = std::find_if(std::cbegin(lockFilePatterns), std::cend(lockFilePatterns), [&candidateUnlockFileName](std::string_view pattern) {
            return candidateUnlockFileName.contains(QString::fromStdString(std::string(pattern)));
        });
        if (lockFilePatternFoundIt != std::cend(lockFilePatterns)) {
            continue;
        }
        if (candidateUnlockFileName.contains(lockFileName)) {
            foundFilePath = candidateUnlockedFileInfo.absoluteFilePath();
            break;
        }
    }
    return foundFilePath;
}
}

namespace OCC {
    
QString FileSystem::filePathLockFilePatternMatch(const QString &path)
{
    qCDebug(OCC::lcFileSystem) << "Checking if it is a lock file:" << path;

    const auto pathSplit = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (pathSplit.isEmpty()) {
        return {};
    }
    QString lockFilePatternFound;
    for (const auto &lockFilePattern : lockFilePatterns) {
        if (pathSplit.last().startsWith(lockFilePattern)) {
            lockFilePatternFound = lockFilePattern;
            break;
        }
    }

    if (!lockFilePatternFound.isEmpty()) {
        qCDebug(OCC::lcFileSystem) << "Found a lock file with prefix:" << lockFilePatternFound << "in path:" << path;
    }

    return lockFilePatternFound;
}

bool FileSystem::isMatchingOfficeFileExtension(const QString &path)
{
    const auto pathSplit = path.split(QLatin1Char('.'));
    const auto extension = pathSplit.size() > 1 ? pathSplit.last().toStdString() : std::string{};
    return std::find(std::cbegin(officeFileExtensions), std::cend(officeFileExtensions), extension) != std::cend(officeFileExtensions);
}

FileSystem::FileLockingInfo FileSystem::lockFileTargetFilePath(const QString &lockFilePath, const QString &lockFileNamePattern)
{
    FileLockingInfo result;

    if (lockFileNamePattern.isEmpty()) {
        return result;
    }

    const auto lockFilePathWithoutPrefix = QString(lockFilePath).replace(lockFileNamePattern, QStringLiteral(""));
    auto lockFilePathWithoutPrefixSplit = lockFilePathWithoutPrefix.split(QLatin1Char('.'));

    if (lockFilePathWithoutPrefixSplit.size() < 2) {
        return result;
    }

    auto extensionSanitized = lockFilePathWithoutPrefixSplit.takeLast().toStdString();
    // remove possible non-alphabetical characters at the end of the extension
    extensionSanitized.erase(std::remove_if(extensionSanitized.begin(),
                                            extensionSanitized.end(),
                                            [](const auto &ch) {
                                                return !std::isalnum(ch);
                                            }),
                             extensionSanitized.end());

    lockFilePathWithoutPrefixSplit.push_back(QString::fromStdString(extensionSanitized));
    const auto lockFilePathWithoutPrefixNew = lockFilePathWithoutPrefixSplit.join(QLatin1Char('.'));

    qCDebug(lcFileSystem) << "Assumed locked/unlocked file path" << lockFilePathWithoutPrefixNew << "Going to try to find matching file";
    auto splitFilePath = lockFilePathWithoutPrefixNew.split(QLatin1Char('/'));
    if (splitFilePath.size() > 1) {
        const auto lockFileNameWithoutPrefix = splitFilePath.takeLast();
        // some software will modify lock file name such that it does not correspond to original file (removing some symbols from the name, so we will
        // search for a matching file
        result.path = findMatchingUnlockedFileInDir(splitFilePath.join(QLatin1Char('/')), lockFileNameWithoutPrefix);
    }

    if (result.path.isEmpty() || !QFile::exists(result.path)) {
        result.path.clear();
        return result;
    }
    result.type = QFile::exists(lockFilePath) ? FileLockingInfo::Type::Locked : FileLockingInfo::Type::Unlocked;
    return result;
}

QStringList FileSystem::findAllLockFilesInDir(const QString &dirPath)
{
    QStringList results;
    const QDir dir(dirPath);
    const auto entryList = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const auto &candidateLockFile : entryList) {
        const auto filePath = candidateLockFile.filePath();
        const auto isLockFile = !filePathLockFilePatternMatch(filePath).isEmpty();
        if (isLockFile) {
            results.push_back(filePath);
        }
    }

    return results;
}

bool FileSystem::fileEquals(const QString &fn1, const QString &fn2)
{
    // compare two files with given filename and return true if they have the same content
    QFile f1(fn1);
    QFile f2(fn2);
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
        qCWarning(lcFileSystem) << "fileEquals: Failed to open " << fn1 << "or" << fn2;
        return false;
    }

    if (getSize(fn1) != getSize(fn2)) {
        return false;
    }

    const int BufferSize = 16 * 1024;
    QByteArray buffer1(BufferSize, 0);
    QByteArray buffer2(BufferSize, 0);
    // the files have the same size, compare all of it
    while(!f1.atEnd()){
        f1.read(buffer1.data(), BufferSize);
        f2.read(buffer2.data(), BufferSize);
        if (buffer1 != buffer2) {
            return false;
        }
    };
    return true;
}

time_t FileSystem::getModTime(const QString &filename)
{
    csync_file_stat_t stat;
    time_t result = -1;
    if (csync_vio_local_stat(filename, &stat) != -1 && (stat.modtime != 0)) {
        result = stat.modtime;
    } else {
        result = Utility::qDateTimeToTime_t(QFileInfo(filename).lastModified());
        qCWarning(lcFileSystem) << "Could not get modification time for" << filename
                                << "with csync, using QFileInfo:" << result;
    }
    return result;
}

bool FileSystem::setModTime(const QString &filename, time_t modTime)
{
    struct timeval times[2];
    times[0].tv_sec = times[1].tv_sec = modTime;
    times[0].tv_usec = times[1].tv_usec = 0;
    int rc = c_utimes(filename, times);
    if (rc != 0) {
        qCWarning(lcFileSystem) << "Error setting mtime for" << filename
                                << "failed: rc" << rc << ", errno:" << errno;
        return false;
    }
    return true;
}

bool FileSystem::fileChanged(const QString &fileName,
    qint64 previousSize,
    time_t previousMtime)
{
    return getSize(fileName) != previousSize
        || getModTime(fileName) != previousMtime;
}

bool FileSystem::verifyFileUnchanged(const QString &fileName,
                                     qint64 previousSize,
                                     time_t previousMtime)
{
    const auto actualSize = getSize(fileName);
    const auto actualMtime = getModTime(fileName);
    if ((actualSize != previousSize && actualMtime > 0) || (actualMtime != previousMtime && previousMtime > 0 && actualMtime > 0)) {
        qCInfo(lcFileSystem) << "File" << fileName << "has changed:"
                             << "size: " << previousSize << "<->" << actualSize
                             << ", mtime: " << previousMtime << "<->" << actualMtime;
        return false;
    }
    return true;
}

#ifdef Q_OS_WIN
static qint64 getSizeWithCsync(const QString &filename)
{
    qint64 result = 0;
    csync_file_stat_t stat;
    if (csync_vio_local_stat(filename, &stat) != -1) {
        result = stat.size;
    } else {
        qCWarning(lcFileSystem) << "Could not get size for" << filename << "with csync" << Utility::formatWinError(errno);
    }
    return result;
}
#endif

qint64 FileSystem::getSize(const QString &filename)
{
#ifdef Q_OS_WIN
    if (isLnkFile(filename)) {
        // Qt handles .lnk as symlink... https://doc.qt.io/qt-5/qfileinfo.html#details
        return getSizeWithCsync(filename);
    }
#endif
    return QFileInfo(filename).size();
}

// Code inspired from Qt5's QDir::removeRecursively
bool FileSystem::removeRecursively(const QString &path, const std::function<void(const QString &path, bool isDir)> &onDeleted, QStringList *errors)
{
    bool allRemoved = true;
    QDirIterator di(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);

    while (di.hasNext()) {
        di.next();
        const QFileInfo &fi = di.fileInfo();
        bool removeOk = false;
        // The use of isSymLink here is okay:
        // we never want to go into this branch for .lnk files
        bool isDir = FileSystem::isDir(fi.absoluteFilePath()) && !FileSystem::isSymLink(fi.absoluteFilePath()) && !FileSystem::isJunction(fi.absoluteFilePath());
        if (isDir) {
            removeOk = removeRecursively(path + QLatin1Char('/') + di.fileName(), onDeleted, errors); // recursive
        } else {
            QString removeError;

#if !defined(Q_OS_MACOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
            const auto fileInfo = QFileInfo{di.filePath()};
            const auto parentFolderPath = fileInfo.dir().absolutePath();
            const auto parentPermissionsHandler = FileSystem::FilePermissionsRestore{parentFolderPath, FileSystem::FolderPermissions::ReadWrite};
#endif
            removeOk = FileSystem::remove(di.filePath(), &removeError);
            if (removeOk) {
                if (onDeleted)
                    onDeleted(di.filePath(), false);
            } else {
                if (errors) {
                    errors->append(QCoreApplication::translate("FileSystem", "Error removing \"%1\": %2")
                                       .arg(QDir::toNativeSeparators(di.filePath()), removeError));
                }
                qCWarning(lcFileSystem) << "Error removing " << di.filePath() << ':' << removeError;
            }
        }
        if (!removeOk)
            allRemoved = false;
    }
    if (allRemoved) {
#if !defined(Q_OS_MACOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
        const auto fileInfo = QFileInfo{path};
        const auto parentFolderPath = fileInfo.dir().absolutePath();
        const auto parentPermissionsHandler = FileSystem::FilePermissionsRestore{parentFolderPath, FileSystem::FolderPermissions::ReadWrite};
        FileSystem::setFolderPermissions(path, FileSystem::FolderPermissions::ReadWrite);
#endif
        allRemoved = QDir().rmdir(path);
        if (allRemoved) {
            if (onDeleted)
                onDeleted(path, true);
        } else {
            if (errors) {
                errors->append(QCoreApplication::translate("FileSystem", "Could not remove folder \"%1\"")
                                   .arg(QDir::toNativeSeparators(path)));
            }
            qCWarning(lcFileSystem) << "Error removing folder" << path;
        }
    }
    return allRemoved;
}

bool FileSystem::getInode(const QString &filename, quint64 *inode)
{
    csync_file_stat_t fs;
    if (csync_vio_local_stat(filename, &fs) == 0) {
        *inode = fs.inode;
        return true;
    }
    return false;
}

#if !defined(Q_OS_MACOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
bool FileSystem::setFolderPermissions(const QString &path,
                                      FileSystem::FolderPermissions permissions) noexcept
{
    static constexpr auto writePerms = std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write;
    const auto stdStrPath = path.toStdWString();
    try
    {
        switch (permissions) {
        case OCC::FileSystem::FolderPermissions::ReadOnly:
            std::filesystem::permissions(stdStrPath, writePerms, std::filesystem::perm_options::remove);
            break;
        case OCC::FileSystem::FolderPermissions::ReadWrite:
            break;
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when modifying folder permissions" << e.what() << "- path1:" << e.path1().c_str() << "- path2:" << e.path2().c_str();
        return false;
    }
    catch (const std::system_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when modifying folder permissions" << e.what() << "- path:" << stdStrPath;
        return false;
    }
    catch (...)
    {
        qCWarning(lcFileSystem()) << "exception when modifying folder permissions -  path:" << stdStrPath;
        return false;
    }

#ifdef Q_OS_WIN
    SECURITY_INFORMATION info = DACL_SECURITY_INFORMATION;
    std::unique_ptr<char[]> securityDescriptor;
    auto neededLength = 0ul;

    if (!GetFileSecurityW(path.toStdWString().c_str(), info, nullptr, 0, &neededLength)) {
        const auto lastError = GetLastError();
        if (lastError != ERROR_INSUFFICIENT_BUFFER) {
            qCWarning(lcFileSystem) << "error when calling GetFileSecurityW" << path << lastError;
            return false;
        }

        securityDescriptor.reset(new char[neededLength]);

        if (!GetFileSecurityW(path.toStdWString().c_str(), info, securityDescriptor.get(), neededLength, &neededLength)) {
            qCWarning(lcFileSystem) << "error when calling GetFileSecurityW" << path << GetLastError();
            return false;
        }
    }

    int daclPresent = false, daclDefault = false;
    PACL resultDacl = nullptr;
    if (!GetSecurityDescriptorDacl(securityDescriptor.get(), &daclPresent, &resultDacl, &daclDefault)) {
        qCWarning(lcFileSystem) << "error when calling GetSecurityDescriptorDacl" << path << GetLastError();
        return false;
    }
    if (!daclPresent || !resultDacl) {
        qCWarning(lcFileSystem) << "error when calling DACL needed to set a folder read-only or read-write is missing" << path;
        return false;
    }

    PSID sid = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-5-32-545", &sid))
    {
        qCWarning(lcFileSystem) << "error when calling ConvertStringSidToSidA" << path << GetLastError();
        return false;
    }

    ACL_SIZE_INFORMATION aclSize;
    if (!GetAclInformation(resultDacl, &aclSize, sizeof(aclSize), AclSizeInformation)) {
        qCWarning(lcFileSystem) << "error when calling GetAclInformation" << path << GetLastError();
        return false;
    }

    const auto newAclSize = aclSize.AclBytesInUse + sizeof(ACCESS_DENIED_ACE) + GetLengthSid(sid);
    qCDebug(lcFileSystem) << "allocated a new DACL object of size" << newAclSize;

    std::unique_ptr<ACL> newDacl{reinterpret_cast<PACL>(new char[newAclSize])};
    if (!InitializeAcl(newDacl.get(), newAclSize, ACL_REVISION)) {
        const auto lastError = GetLastError();
        if (lastError != ERROR_INSUFFICIENT_BUFFER) {
            qCWarning(lcFileSystem) << "insufficient memory error when calling InitializeAcl" << path;
            return false;
        }

        qCWarning(lcFileSystem) << "error when calling InitializeAcl" << path << lastError;
        return false;
    }

    if (permissions == FileSystem::FolderPermissions::ReadOnly) {
        qCInfo(lcFileSystem) << path << "will be read only";
        if (!AddAccessDeniedAce(newDacl.get(), ACL_REVISION, FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_APPEND_DATA | FILE_DELETE_CHILD, sid)) {
            qCWarning(lcFileSystem) << "error when calling AddAccessDeniedAce << path" << GetLastError();
            return false;
        }
    }

    for (int i = 0; i < aclSize.AceCount; ++i) {
        void *currentAce = nullptr;
        if (!GetAce(resultDacl, i, &currentAce)) {
            qCWarning(lcFileSystem) << "error when calling GetAce" << path << GetLastError();
            return false;
        }

        const auto currentAceHeader = reinterpret_cast<PACE_HEADER>(currentAce);

        if (permissions == FileSystem::FolderPermissions::ReadWrite) {
            qCInfo(lcFileSystem) << path << "will be read write";
        }
        if (permissions == FileSystem::FolderPermissions::ReadWrite && (ACCESS_DENIED_ACE_TYPE == (currentAceHeader->AceType & ACCESS_DENIED_ACE_TYPE))) {
            qCWarning(lcFileSystem) << "AceHeader" << path << currentAceHeader->AceFlags << currentAceHeader->AceSize << currentAceHeader->AceType;
            continue;
        }

        if (!AddAce(newDacl.get(), ACL_REVISION, i + 1, currentAce, currentAceHeader->AceSize)) {
            const auto lastError = GetLastError();
            if (lastError != ERROR_INSUFFICIENT_BUFFER) {
                qCWarning(lcFileSystem) << "insufficient memory error when calling AddAce" << path;
                return false;
            }

            if (lastError != ERROR_INVALID_PARAMETER) {
                qCWarning(lcFileSystem) << "invalid parameter error when calling AddAce" << path << "ACL size" << newAclSize;
                return false;
            }

            qCWarning(lcFileSystem) << "error when calling AddAce" << path << lastError << "acl index" << (i + 1);
            return false;
        }
    }

    SECURITY_DESCRIPTOR newSecurityDescriptor;
    if (!InitializeSecurityDescriptor(&newSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION)) {
        qCWarning(lcFileSystem) << "error when calling InitializeSecurityDescriptor" << path << GetLastError();
        return false;
    }

    if (!SetSecurityDescriptorDacl(&newSecurityDescriptor, true, newDacl.get(), false)) {
        qCWarning(lcFileSystem) << "error when calling SetSecurityDescriptorDacl" << path << GetLastError();
        return false;
    }

    if (!SetFileSecurityW(path.toStdWString().c_str(), info, &newSecurityDescriptor)) {
        qCWarning(lcFileSystem) << "error when calling SetFileSecurityW" << path << GetLastError();
        return false;
    }
#endif

    try
    {
        switch (permissions) {
        case OCC::FileSystem::FolderPermissions::ReadOnly:
            break;
        case OCC::FileSystem::FolderPermissions::ReadWrite:
            std::filesystem::permissions(stdStrPath, std::filesystem::perms::others_write, std::filesystem::perm_options::remove);
            std::filesystem::permissions(stdStrPath, std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
            break;
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when modifying folder permissions" << e.what() << e.path1().c_str() << e.path2().c_str();
        return false;
    }
    catch (const std::system_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when modifying folder permissions" << e.what() << "- path:" << stdStrPath;
        return false;
    }
    catch (...)
    {
        qCWarning(lcFileSystem()) << "exception when modifying folder permissions -  path:" << stdStrPath;
        return false;
    }

    return true;
}

bool FileSystem::isFolderReadOnly(const std::filesystem::path &path) noexcept
{
    try
    {
        const auto folderStatus = std::filesystem::status(path);
        const auto folderPermissions = folderStatus.permissions();
        return (folderPermissions & std::filesystem::perms::owner_write) != std::filesystem::perms::owner_write;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when checking folder permissions" << e.what() << "- path1:" << e.path1().c_str() << "- path2:" << e.path2().c_str();
        return false;
    }
    catch (const std::system_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when checking folder permissions" << e.what() << "- path:" << path;
        return false;
    }
    catch (...)
    {
        qCWarning(lcFileSystem()) << "exception when checking folder permissions -  path:" << path;
        return false;
    }
}

FileSystem::FilePermissionsRestore::FilePermissionsRestore(const QString &path, FolderPermissions temporaryPermissions)
    : _path(path)
{
    try
    {
        const auto stdStrPath = _path.toStdWString();
        _initialPermissions = FileSystem::isFolderReadOnly(stdStrPath) ? OCC::FileSystem::FolderPermissions::ReadOnly : OCC::FileSystem::FolderPermissions::ReadWrite;
        if (_initialPermissions != temporaryPermissions) {
            _rollbackNeeded = true;
            FileSystem::setFolderPermissions(_path, temporaryPermissions);
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when modifying folder permissions" << e.what() << "- path1:" << e.path1().c_str() << "- path2:" << e.path2().c_str();
    }
    catch (const std::system_error &e)
    {
        qCWarning(lcFileSystem()) << "exception when modifying folder permissions" << e.what() << "- path:" << path;
    }
    catch (...)
    {
        qCWarning(lcFileSystem()) << "exception when modifying folder permissions -  path:" << path;
    }
}

FileSystem::FilePermissionsRestore::~FilePermissionsRestore()
{
    if (_rollbackNeeded) {
        FileSystem::setFolderPermissions(_path, _initialPermissions);
    }
}

#endif

} // namespace OCC
