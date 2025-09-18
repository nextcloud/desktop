/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "config.h"

#include "owncloudlib.h"
#include "common/filesystembase.h"

#include <QString>
#include <QStringList>

#include <ctime>
#include <functional>
#include <functional>

class QFile;

namespace OCC {

class SyncJournal;

/**
 *  \addtogroup libsync
 *  @{
 */

/**
 * @brief This file contains file system helper
 */
namespace FileSystem {
    class OWNCLOUDSYNC_EXPORT FilePermissionsRestore {
    public:
        explicit FilePermissionsRestore(const QString &path,
                                        FileSystem::FolderPermissions temporaryPermissions);

        ~FilePermissionsRestore();

    private:
        QString _path;
        FileSystem::FolderPermissions _initialPermissions;
        bool _rollbackNeeded = false;
    };

    struct OWNCLOUDSYNC_EXPORT FileLockingInfo {
        enum class Type { Unset = -1, Locked, Unlocked };
        QString path;
        Type type = Type::Unset;
    };

    // match file path with lock pattern
    QString OWNCLOUDSYNC_EXPORT filePathLockFilePatternMatch(const QString &path);
    // check if it is an office file (by extension), ONLY call it for files
    bool OWNCLOUDSYNC_EXPORT isMatchingOfficeFileExtension(const QString &path);
    // finds and fetches FileLockingInfo for the corresponding file that we are locking/unlocking
    FileLockingInfo OWNCLOUDSYNC_EXPORT lockFileTargetFilePath(const QString &lockFilePath, const QString &lockFileNamePattern);
    // lists all files matching a lockfile pattern in dirPath
    QStringList OWNCLOUDSYNC_EXPORT findAllLockFilesInDir(const QString &dirPath);

    /**
     * @brief compare two files with given filename and return true if they have the same content
     */
    bool fileEquals(const QString &fn1, const QString &fn2);

    /**
     * @brief Get the mtime for a filepath
     *
     * Use this over QFileInfo::lastModified() to avoid timezone related bugs. See
     * owncloud/core#9781 for details.
     */
    time_t OWNCLOUDSYNC_EXPORT getModTime(const QString &filename);

    bool OWNCLOUDSYNC_EXPORT setModTime(const QString &filename, time_t modTime);

    /**
     * @brief Get the size for a file
     *
     * Use this over QFileInfo::size() to avoid bugs with lnk files on Windows.
     * See https://bugreports.qt.io/browse/QTBUG-24831.
     */
    qint64 OWNCLOUDSYNC_EXPORT getSize(const QString &filename);

    /**
     * @brief Retrieve a file inode with csync
     */
    bool OWNCLOUDSYNC_EXPORT getInode(const QString &filename, quint64 *inode);

    /**
     * @brief Check if \a fileName has changed given previous size and mtime
     *
     * Nonexisting files are covered through mtime: they have an mtime of -1.
     *
     * @return true if the file's mtime or size are not what is expected.
     */
    bool OWNCLOUDSYNC_EXPORT fileChanged(const QString &fileName,
        qint64 previousSize,
        time_t previousMtime);
    /**
     * @brief Like !fileChanged() but with verbose logging if the file *did* change.
     */
    bool OWNCLOUDSYNC_EXPORT verifyFileUnchanged(const QString &fileName,
        qint64 previousSize,
        time_t previousMtime);

    /**
     * Removes a directory and its contents recursively
     *
     * Returns true if all removes succeeded.
     * onDeleted() is called for each deleted file or directory, including the root.
     * errors are collected in errors.
     */
    bool OWNCLOUDSYNC_EXPORT removeRecursively(const QString &path,
                                               const std::function<void(const QString &path, bool isDir)> &onDeleted = nullptr,
                                               QStringList *errors = nullptr,
                                               const std::function<void(const QString &path, bool isDir)> &onError = nullptr);

    bool OWNCLOUDSYNC_EXPORT setFolderPermissions(const QString &path,
                                                  FileSystem::FolderPermissions permissions,
                                                  bool *permissionsChanged = nullptr) noexcept;

    bool OWNCLOUDSYNC_EXPORT isFolderReadOnly(const std::filesystem::path &path) noexcept;

    /**
     * Rename the file \a originFileName to \a destinationFileName, and
     * overwrite the destination if it already exists - without extra checks.
     */
    bool OWNCLOUDSYNC_EXPORT uncheckedRenameReplace(const QString &originFileName,
                                                    const QString &destinationFileName,
                                                    QString *errorString);
}

/** @} */
}
