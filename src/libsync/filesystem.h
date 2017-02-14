/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#pragma once

#include "config.h"

#include <QString>
#include <ctime>
#include <QCryptographicHash>
#include <QFileInfo>

#include <owncloudlib.h>

class QFile;
class QFileInfo;

namespace OCC {

/**
 *  \addtogroup libsync
 *  @{
 */

/**
 * @brief This file contains file system helper
 */
namespace FileSystem {

/**
 * @brief compare two files with given filename and return true if they have the same content
 */
bool fileEquals(const QString &fn1, const QString &fn2);

/**
 * @brief Mark the file as hidden  (only has effects on windows)
 */
void OWNCLOUDSYNC_EXPORT setFileHidden(const QString& filename, bool hidden);

/**
 * @brief Marks the file as read-only.
 *
 * On linux this either revokes all 'w' permissions or restores permissions
 * according to the umask.
 */
void OWNCLOUDSYNC_EXPORT setFileReadOnly(const QString& filename, bool readonly);

/**
 * @brief Marks the file as read-only.
 *
 * It's like setFileReadOnly(), but weaker: if readonly is false and the user
 * already has write permissions, no change to the permissions is made.
 *
 * This means that it will preserve explicitly set rw-r--r-- permissions even
 * when the umask is 0002. (setFileReadOnly() would adjust to rw-rw-r--)
 */
void OWNCLOUDSYNC_EXPORT setFileReadOnlyWeak(const QString& filename, bool readonly);

/**
 * @brief Try to set permissions so that other users on the local machine can not
 * go into the folder.
 */
void OWNCLOUDSYNC_EXPORT setFolderMinimumPermissions(const QString& filename);

/** convert a "normal" windows path into a path that can be 32k chars long. */
QString OWNCLOUDSYNC_EXPORT longWinPath( const QString& inpath );

/**
 * @brief Get the mtime for a filepath
 *
 * Use this over QFileInfo::lastModified() to avoid timezone related bugs. See
 * owncloud/core#9781 for details.
 */
time_t OWNCLOUDSYNC_EXPORT getModTime(const QString& filename);

bool OWNCLOUDSYNC_EXPORT setModTime(const QString &filename, time_t modTime);

/**
 * @brief Get the size for a file
 *
 * Use this over QFileInfo::size() to avoid bugs with lnk files on Windows.
 * See https://bugreports.qt.io/browse/QTBUG-24831.
 */
qint64 OWNCLOUDSYNC_EXPORT getSize(const QString& filename);

/**
 * @brief Checks whether a file exists.
 *
 * Use this over QFileInfo::exists() and QFile::exists() to avoid bugs with lnk
 * files, see above.
 */
bool OWNCLOUDSYNC_EXPORT fileExists(const QString& filename,  const QFileInfo& = QFileInfo() );

/**
 * @brief Rename the file \a originFileName to \a destinationFileName.
 *
 * It behaves as QFile::rename() but handles .lnk files correctly on Windows.
 */
bool OWNCLOUDSYNC_EXPORT rename(const QString& originFileName,
                                const QString& destinationFileName,
                                QString* errorString = NULL);

/**
 * @brief Check if \a fileName has changed given previous size and mtime
 *
 * Nonexisting files are covered through mtime: they have an mtime of -1.
 *
 * @return true if the file's mtime or size are not what is expected.
 */
bool OWNCLOUDSYNC_EXPORT fileChanged(const QString& fileName,
                                     qint64 previousSize,
                                     time_t previousMtime);

/**
 * @brief Like !fileChanged() but with verbose logging if the file *did* change.
 */
bool verifyFileUnchanged(const QString& fileName,
                         qint64 previousSize,
                         time_t previousMtime);

/**
 * @brief renames a file, overriding the target if it exists
 *
 * Rename the file \a originFileName to \a destinationFileName, and
 * overwrite the destination if it already exists - as long as the
 * destination file has the expected \a destinationSize and
 * \a destinationMtime.
 *
 * If the destination file does not exist, the given size and mtime are
 * ignored.
 */
bool renameReplace(const QString &originFileName,
                   const QString &destinationFileName,
                   qint64 destinationSize,
                   time_t destinationMtime,
                   QString *errorString);

/**
 * Rename the file \a originFileName to \a destinationFileName, and
 * overwrite the destination if it already exists - without extra checks.
 */
bool uncheckedRenameReplace(const QString &originFileName,
                            const QString &destinationFileName,
                            QString *errorString);

/**
 * Removes a file.
 *
 * Equivalent to QFile::remove(), except on Windows, where it will also
 * successfully remove read-only files.
 */
bool OWNCLOUDSYNC_EXPORT remove(const QString &fileName, QString *errorString = 0);

/**
 * Replacement for QFile::open(ReadOnly) followed by a seek().
 * This version sets a more permissive sharing mode on Windows.
 *
 * Warning: The resulting file may have an empty fileName and be unsuitable for use
 * with QFileInfo! Calling seek() on the QFile with >32bit signed values will fail!
 */
bool openAndSeekFileSharedRead(QFile* file, QString* error, qint64 seek);

#ifdef Q_OS_WIN
/**
 * Returns the file system used at the given path.
 */
QString fileSystemForPath(const QString & path);
#endif

QByteArray OWNCLOUDSYNC_EXPORT calcMd5( const QString& fileName );
QByteArray OWNCLOUDSYNC_EXPORT calcSha1( const QString& fileName );
#ifdef ZLIB_FOUND
QByteArray OWNCLOUDSYNC_EXPORT calcAdler32( const QString& fileName );
#endif

/**
 * Returns a file name based on \a fn that's suitable for a conflict.
 */
QString OWNCLOUDSYNC_EXPORT makeConflictFileName(const QString &fn, const QDateTime &dt);

/**
 * Returns true when a file is locked. (Windows only)
 */
bool OWNCLOUDSYNC_EXPORT isFileLocked(const QString& fileName);

}

/** @} */

}
