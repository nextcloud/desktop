/*
 * Copyright (C) by Christian Kamm <mail@ckamm.com>
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

#include "owncloudlib.h"

#include <QObject>
#include <QReadWriteLock>
#include <QStringList>

extern "C" {
#include "std/c_string.h"
#include "csync.h"
#include "csync_exclude.h" // for CSYNC_EXCLUDE_TYPE
}

namespace OCC {

/**
 * Manages the global system and user exclude lists.
 */
class OWNCLOUDSYNC_EXPORT ExcludedFiles : public QObject
{
    Q_OBJECT
public:
    static ExcludedFiles & instance();

    /**
     * Adds a new path to a file containing exclude patterns.
     *
     * Does not load the file. Use reloadExcludes() afterwards.
     */
    void addExcludeFilePath(const QString& path);

    /**
     * Checks whether a file or directory should be excluded.
     *
     * @param fullPath     the absolute path to the file
     * @param relativePath path relative to the folder
     *
     * For directories, the paths must not contain a trailing /.
     */
    CSYNC_EXCLUDE_TYPE isExcluded(
            const QString& fullPath,
            const QString& relativePath,
            bool excludeHidden) const;

public slots:
    /**
     * Reloads the exclude patterns from the registered paths.
     */
    void reloadExcludes();

private:
    ExcludedFiles();
    ~ExcludedFiles();

    c_strlist_t* _excludes;
    QStringList _excludeFiles;
    mutable QReadWriteLock _mutex;
};

} // namespace OCC
