/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#pragma once

#include "ocsynclib.h"

#include "utility.h"

#include <QFileInfo>
#include <QLoggingCategory>
#include <QString>

#include <cstdint>
#include <ctime>


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
    OCSYNC_EXPORT Q_NAMESPACE;

    /**
     * @brief Mark the file as hidden  (only has effects on windows)
     */
    void OCSYNC_EXPORT setFileHidden(const QString &filename, bool hidden);

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
    void OCSYNC_EXPORT setFileReadOnlyWeak(const QString &filename, bool readonly);

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
     * @brief Rename the file \a originFileName to \a destinationFileName.
     *
     * It behaves as QFile::rename() but handles .lnk files correctly on Windows.
     */
    bool OCSYNC_EXPORT rename(const QString &originFileName,
        const QString &destinationFileName,
        QString *errorString = nullptr);

    /**
     * Rename the file \a originFileName to \a destinationFileName, and
     * overwrite the destination if it already exists - without extra checks.
     */
    bool OCSYNC_EXPORT uncheckedRenameReplace(const QString &originFileName,
        const QString &destinationFileName,
        QString *errorString);

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
    bool OCSYNC_EXPORT openAndSeekFileSharedRead(QFile * file, QString * error, qint64 seek);

    enum class LockMode {
        Shared,
        Exclusive
    };
    Q_ENUM_NS(LockMode);
    /**
     * Returns true when a file is locked. (Windows only)
     */
    bool OCSYNC_EXPORT isFileLocked(const QString &fileName, LockMode mode);

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

    /**
     * This function creates a file handle with the desired LockMode
     */
    Utility::Handle OCSYNC_EXPORT lockFile(const QString &fileName, LockMode mode);
#endif

    /**
     * Returns whether the file is a shortcut file (ends with .lnk)
     */
    bool OCSYNC_EXPORT isLnkFile(const QString &filename);

    /**
     * Returns whether the file is a junction (windows only)
     */
    bool OCSYNC_EXPORT isJunction(const QString &filename);

    /**
     * Returns whether a Path is a child of another
     */
    bool OCSYNC_EXPORT isChildPathOf(const QString &child, const QString &parent);


    namespace SizeLiterals {
        constexpr unsigned long long operator"" _b(unsigned long long sz)
        {
            return sz;
        }

        constexpr unsigned long long operator"" _kb(unsigned long long sz)
        {
            return operator"" _b(sz) * 1024;
        }

        constexpr unsigned long long operator"" _mb(unsigned long long sz)
        {
            return operator"" _kb(sz) * 1024;
        }

        constexpr unsigned long long operator"" _gb(unsigned long long sz)
        {
            return operator"" _mb(sz) * 1024;
        }
    }
}

/** @} */
}
