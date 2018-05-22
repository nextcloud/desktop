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

#include <owncloudlib.h>
// Chain in the base include and extend the namespace
#include "common/filesystembase.h"

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
    bool verifyFileUnchanged(const QString &fileName,
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
        QStringList *errors = nullptr);
}

/** @} */
}
