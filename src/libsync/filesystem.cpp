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

#include "common/asserts.h"
#include "common/utility.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include "csync.h"
#include "vio/csync_vio_local.h"
#include "std/c_time.h"

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <sys/xattr.h>
#endif

#ifdef Q_OS_WIN32
#include "common/utility_win.h"
#include <winsock2.h>
#endif

namespace {
static constexpr unsigned MaxValueSize = 1023; // This is without a terminating NUL character
} // anonymous namespace

namespace OCC {

bool FileSystem::fileEquals(const QString &fn1, const QString &fn2)
{
    // compare two files with given filename and return true if they have the same content
    QFile f1(fn1);
    QFile f2(fn2);
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
        qCWarning(lcFileSystem) << "fileEquals: Failed to open " << fn1 << "or" << fn2;
        return false;
    }

    if (getSize(QFileInfo{fn1}) != getSize(QFileInfo{fn2})) {
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
    qint64 result = -1;
    if (csync_vio_local_stat(filename, &stat) != -1
        && (stat.modtime != 0)) {
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
                                << "failed: rc" << rc << ", error message:" << strerror(errno);
        Q_ASSERT(false);
        return false;
    }
    return true;
}

bool FileSystem::fileChanged(const QFileInfo &info, qint64 previousSize, time_t previousMtime, std::optional<quint64> previousInode)
{
    // previousMtime == -1 indicates the file does not exist
    if (!info.exists() && previousMtime != -1) {
        qCDebug(lcFileSystem) << info.filePath() << "was removed";
        return true;
    }
    const qint64 actualSize = getSize(info);
    if (actualSize != previousSize) {
        qCDebug(lcFileSystem) << "File" << info.filePath() << "has changed: size: " << previousSize << "<->" << actualSize;
        return true;
    } else {
        const time_t actualMtime = getModTime(info.filePath());
        if (actualMtime != previousMtime) {
            qCDebug(lcFileSystem) << "File" << info.filePath() << "has changed: mtime: " << previousMtime << "<->" << actualMtime;
            return true;
        } else if (previousInode.has_value()) {
            quint64 actualIndoe;
            FileSystem::getInode(info.filePath(), &actualIndoe);
            if (previousInode.value() != actualIndoe) {
                qCDebug(lcFileSystem) << "File" << info.filePath() << "has changed: inode" << previousInode.value() << "<-->" << actualIndoe;
                return true;
            }
        }
    }
    return false;
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

qint64 FileSystem::getSize(const QFileInfo &info)
{
#ifdef Q_OS_WIN
    if (isLnkFile(info.fileName())) {
        // Qt handles .lnk as symlink... https://doc.qt.io/qt-5/qfileinfo.html#details
        return getSizeWithCsync(info.filePath());
    }
#endif
    return info.size();
}

// Code inspired from Qt5's QDir::removeRecursively
bool FileSystem::removeRecursively(const QString &path,
    RemoveEntryList *success,
    RemoveEntryList *locked,
    RemoveErrorList *errors)
{
    bool allRemoved = true;
    QDirIterator di(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);

    QString removeError;
    while (di.hasNext()) {
        di.next();
        const QFileInfo &fi = di.fileInfo();
        // The use of isSymLink here is okay:
        // we never want to go into this branch for .lnk files
        const bool isDir = fi.isDir() && !fi.isSymLink() && !FileSystem::isJunction(fi.absoluteFilePath());
        if (isDir) {
            allRemoved &= removeRecursively(path + QLatin1Char('/') + di.fileName(), success, locked, errors); // recursive
        } else {
            if (FileSystem::isFileLocked(di.filePath(), FileSystem::LockMode::Exclusive)) {
                locked->push_back({ di.filePath(), isDir });
                allRemoved = false;
            } else {
                if (FileSystem::remove(di.filePath(), &removeError)) {
                    success->push_back({ di.filePath(), isDir });
                } else {
                    errors->push_back({ { di.filePath(), isDir }, removeError });
                    qCWarning(lcFileSystem) << "Error removing " << di.filePath() << ':' << removeError;
                    allRemoved = false;
                }
            }
        }
    }
    if (allRemoved) {
        allRemoved = QDir().rmdir(path);
        if (allRemoved) {
            success->push_back({ path, true });
        } else {
            errors->push_back({ { path, true }, QCoreApplication::translate("FileSystem", "Could not remove folder") });
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

namespace {

#ifdef Q_OS_LINUX
    Q_ALWAYS_INLINE ssize_t getxattr(const char *path, const char *name, void *value, size_t size, u_int32_t, int)
    {
        return ::getxattr(path, name, value, size);
    }

    Q_ALWAYS_INLINE int setxattr(const char *path, const char *name, const void *value, size_t size, u_int32_t, int)
    {
        return ::setxattr(path, name, value, size, 0);
    }

    Q_ALWAYS_INLINE int removexattr(const char *path, const char *name, int)
    {
        return ::removexattr(path, name);
    }
#endif // Q_OS_LINUX

} // anonymous namespace

std::optional<QByteArray> FileSystem::Tags::get(const QString &path, const QString &key)
{
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    QString platformKey = key;
    if (Utility::isLinux()) {
        platformKey = QStringLiteral("user.") + platformKey;
    }

    QByteArray value(MaxValueSize + 1, '\0'); // Add a NUL character to terminate a string
    auto size = getxattr(path.toUtf8().constData(), platformKey.toUtf8().constData(), value.data(), MaxValueSize, 0, 0);
    if (size != -1) {
        value.truncate(size);
        return value;
    }
#elif defined(Q_OS_WIN)
    QFile file(QStringLiteral("%1:%2").arg(path, key));
    if (file.open(QIODevice::ReadOnly)) {
        return file.readAll();
    }
#endif // Q_OS_MAC || Q_OS_LINUX

    return {};
}

OCC::Result<void, QString> FileSystem::Tags::set(const QString &path, const QString &key, const QByteArray &value)
{
    OC_ASSERT(value.size() < MaxValueSize)

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    QString platformKey = key;
    if (Utility::isLinux()) {
        platformKey = QStringLiteral("user.") + platformKey;
    }

    auto result = setxattr(path.toUtf8().constData(), platformKey.toUtf8().constData(), value.constData(), value.size(), 0, 0);
    if (result != 0) {
        return QString::fromUtf8(strerror(errno));
    }

    return {};
#elif defined(Q_OS_WIN)
    QFile file(QStringLiteral("%1:%2").arg(path, key));
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    auto bytesWritten = file.write(value);
    if (bytesWritten != value.size()) {
        return QStringLiteral("wrote %1 out of %2 bytes").arg(QString::number(bytesWritten), QString::number(value.size()));
    }

    return {};
#else
    return QStringLiteral("function not implemented");
#endif // Q_OS_MAC || Q_OS_LINUX
}

bool FileSystem::Tags::remove(const QString &path, const QString &key)
{
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    QString platformKey = key;
    if (Utility::isLinux()) {
        platformKey = QStringLiteral("user.%1").arg(platformKey);
    }

    auto result = removexattr(path.toUtf8().constData(), platformKey.toUtf8().constData(), 0);

    return result == 0;
#elif defined(Q_OS_WIN)
    return QFile::remove(QStringLiteral("%1:%2").arg(path, key));
#else
    return false;
#endif // Q_OS_MAC || Q_OS_LINUX
}


} // namespace OCC
