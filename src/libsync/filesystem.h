/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#pragma once

#include "config.h"

#include <QString>
#include <ctime>
#include <QCryptographicHash>

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

/** convert a "normal" windows path into a path that can be 32k chars long. */
QString OWNCLOUDSYNC_EXPORT longWinPath( const QString& inpath );

/**
 * @brief Get the mtime for a filepath
 *
 * Use this over QFileInfo::lastModified() to avoid timezone related bugs. See
 * owncloud/core#9781 for details.
 */
time_t OWNCLOUDSYNC_EXPORT getModTime(const QString& filename);

bool setModTime(const QString &filename, time_t modTime);

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
bool OWNCLOUDSYNC_EXPORT fileExists(const QString& filename);

/**
 * @brief Rename the file \a originFileName to \a destinationFileName.
 *
 * It behaves as QFile::rename() but handles .lnk files correctly on Windows.
 */
bool OWNCLOUDSYNC_EXPORT rename(const QString& originFileName,
                                const QString& destinationFileName,
                                QString* errorString = NULL);

/**
 * @brief Check if \a fileName chas changed given previous size and mtime
 *
 * Nonexisting files are covered through mtime: they have an mtime of -1.
 *
 * @return true if the file's mtime or size are not what is expected.
 */
bool fileChanged(const QString& fileName,
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
 * Replacement for QFile::open(ReadOnly) followed by a seek().
 * This version sets a more permissive sharing mode on Windows.
 *
 * Warning: The resuting file may have an empty fileName and be unsuitable for use
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

}

/** @} */

}
