/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "config.h"

#include "csync/ocsynclib.h"

#include <QString>
#include <QFileInfo>
#include <QLoggingCategory>

#include <ctime>

#if !defined(Q_OS_MACOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
#include <filesystem>
#endif

class QFile;

namespace OCC {

OCSYNC_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcFileSystem)

/**
 *  \addtogroup libsync
 *  @{
 */

/**
 * @brief This file contains file system helper
 */
namespace FileSystem {
    enum class FolderPermissions {
        ReadOnly,
        ReadWrite,
    };

    /**
     * @brief Mark the file as hidden  (only has effects on windows)
     */
    void OCSYNC_EXPORT setFileHidden(const QString &filename, bool hidden);

    bool OCSYNC_EXPORT isFileHidden(const QString &filename);

    /**
     * @brief Marks the file as read-only.
     *
     * On linux this either revokes all 'w' permissions or restores permissions
     * according to the umask.
     */
    void OCSYNC_EXPORT setFileReadOnly(const QString &filename, bool readonly);

    /**
     * @brief Marks the file as read-only.
     *
     * It's like setFileReadOnly(), but weaker: if readonly is false and the user
     * already has write permissions, no change to the permissions is made.
     *
     * This means that it will preserve explicitly set rw-r--r-- permissions even
     * when the umask is 0002. (setFileReadOnly() would adjust to rw-rw-r--)
     */
    bool OCSYNC_EXPORT setFileReadOnlyWeak(const QString &filename, bool readonly);

    /**
     * @brief Try to set permissions so that other users on the local machine can not
     * go into the folder.
     */
    void OCSYNC_EXPORT setFolderMinimumPermissions(const QString &filename);

    /** convert a "normal" windows path into a path that can be 32k chars long. */
    QString OCSYNC_EXPORT longWinPath(const QString &inpath);

    /**
     * @brief Checks whether a file exists.
     *
     * Use this over QFileInfo::exists() and QFile::exists() to avoid bugs with lnk
     * files, see above.
     */
    bool OCSYNC_EXPORT fileExists(const QString &filename, const QFileInfo & = QFileInfo());

    /**
     * @brief Checks whether it is a dir.
     *
     * Use this over QFileInfo::isDir() and QFile::isDir() to avoid bugs with lnk
     * files, see above.
     */
    bool OCSYNC_EXPORT isDir(const QString &filename, const QFileInfo& = QFileInfo());

    /**
     * @brief Checks whether it is a file.
     *
     * Use this over QFileInfo::isDir() and QFile::isDir() to avoid bugs with lnk
     * files, see above.
     */
    bool OCSYNC_EXPORT isFile(const QString &filename, const QFileInfo& fileInfo = QFileInfo());

    /**
     * @brief Checks whether the file is writable.
     *
     * Use this over QFileInfo::isDir() and QFile::isDir() to avoid bugs with lnk
     * files, see above.
     */
    bool OCSYNC_EXPORT isWritable(const QString &filename, const QFileInfo &fileInfo = QFileInfo());

    bool OCSYNC_EXPORT isReadable(const QString &filename, const QFileInfo &fileInfo = QFileInfo());

    bool OCSYNC_EXPORT isSymLink(const QString &filename, const QFileInfo &fileInfo = QFileInfo());

    /**
     * @brief Rename the file \a originFileName to \a destinationFileName.
     *
     * It behaves as QFile::rename() but handles .lnk files correctly on Windows.
     */
    bool OCSYNC_EXPORT rename(const QString &originFileName,
        const QString &destinationFileName,
        QString *errorString = nullptr);

    /**
     * Removes a file.
     *
     * Equivalent to QFile::remove(), except on Windows, where it will also
     * successfully remove read-only files.
     */
    bool OCSYNC_EXPORT remove(const QString &fileName, QString *errorString = nullptr);

    /**
     * Move the specified file or folder to the trash. (Only implemented on linux)
     */
    bool OCSYNC_EXPORT moveToTrash(const QString &filename, QString *errorString);

    /**
     * Replacement for QFile::open(ReadOnly) followed by a seek().
     * This version sets a more permissive sharing mode on Windows.
     *
     * Warning: The resulting file may have an empty fileName and be unsuitable for use
     * with QFileInfo! Calling seek() on the QFile with >32bit signed values will fail!
     */
    bool OCSYNC_EXPORT openAndSeekFileSharedRead(QFile *file, QString *error, qint64 seek);

    /**
     * Returns `path + "/" + file` with native directory separators.
     * 
     * If `path` ends in a directory separator this method will not insert another one in-between.
     *
     * In the case one of the parameters is empty, the other parameter will be returned with native
     * directory separators and a warning is logged.
     */
    QString OCSYNC_EXPORT joinPath(const QString &path, const QString &file);

#ifdef Q_OS_WIN
    /**
     * Returns the file system used at the given path.
     */
    QString fileSystemForPath(const QString &path);

    /*
     * This function takes a path and converts it to a UNC representation of the
     * string. That means that it prepends a \\?\ (unless already UNC) and converts
     * all slashes to backslashes.
     *
     * Note the following:
     *  - The string must be absolute.
     *  - it needs to contain a drive character to be a valid UNC
     *  - A conversion is only done if the path len is larger than 245. Otherwise
     *    the windows API functions work with the normal "unixoid" representation too.
     */
    QString OCSYNC_EXPORT pathtoUNC(const QString &str);

    std::filesystem::perms OCSYNC_EXPORT filePermissionsWinSymlinkSafe(const QString &filename);
    std::filesystem::perms OCSYNC_EXPORT filePermissionsWin(const QString &filename);
    void OCSYNC_EXPORT setFilePermissionsWin(const QString &filename, const std::filesystem::perms &perms);

    bool OCSYNC_EXPORT setAclPermission(const QString &path, FileSystem::FolderPermissions permissions, bool applyAlsoToFiles);
#endif

    /**
     * Returns true when a file is locked. (Windows only)
     */
    bool OCSYNC_EXPORT isFileLocked(const QString &fileName);

    /**
     * Returns whether the file is a shortcut file (ends with .lnk)
     */
    bool OCSYNC_EXPORT isLnkFile(const QString &filename);

    /**
     * Returns whether the file is an exclude file (contains patterns to exclude from sync)
     */
    bool OCSYNC_EXPORT isExcludeFile(const QString &filename);

    /**
     * Returns whether the file is a junction (windows only)
     */
    bool OCSYNC_EXPORT isJunction(const QString &filename);
}

/** @} */
}
