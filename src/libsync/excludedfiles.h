/*
 * Copyright (C) by Christian Kamm <mail@ckamm.com>
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

#include "owncloudlib.h"

#include <QObject>
#include <QSet>
#include <QString>

#include "std/c_string.h"
#include "csync.h"
#include "csync_exclude.h" // for CSYNC_EXCLUDE_TYPE

namespace OCC {

/**
 * Manages the global system and user exclude lists.
 */
class OWNCLOUDSYNC_EXPORT ExcludedFiles : public QObject
{
    Q_OBJECT
public:
    static ExcludedFiles &instance();

    ExcludedFiles(c_strlist_t **excludesPtr);
    ~ExcludedFiles();

    /**
     * Adds a new path to a file containing exclude patterns.
     *
     * Does not load the file. Use reloadExcludes() afterwards.
     */
    void addExcludeFilePath(const QString &path);

    /**
     * Checks whether a file or directory should be excluded.
     *
     * @param filePath     the absolute path to the file
     * @param basePath     folder path from which to apply exclude rules
     */
    bool isExcluded(
        const QString &filePath,
        const QString &basePath,
        bool excludeHidden) const;

#ifdef WITH_TESTING
    void addExcludeExpr(const QString &expr);
#endif

public slots:
    /**
     * Reloads the exclude patterns from the registered paths.
     */
    bool reloadExcludes();

private:
    // This is a pointer to the csync exclude list, its is owned by this class
    // but the pointer can be in a csync_context so that it can itself also query the list.
    c_strlist_t **_excludesPtr;
    QSet<QString> _excludeFiles;
};

} // namespace OCC
