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

#include "excludedfiles.h"

#include <QFileInfo>
#include <QReadLocker>
#include <QWriteLocker>

extern "C" {
#include "std/c_string.h"
#include "csync.h"
#include "csync_exclude.h"
}

using namespace OCC;

ExcludedFiles::ExcludedFiles(c_strlist_t** excludesPtr)
    : _excludesPtr(excludesPtr)
{
}

ExcludedFiles::~ExcludedFiles()
{
    c_strlist_destroy(*_excludesPtr);
}

ExcludedFiles& ExcludedFiles::instance()
{
    static c_strlist_t* globalExcludes;
    static ExcludedFiles inst(&globalExcludes);
    return inst;
}

void ExcludedFiles::addExcludeFilePath(const QString& path)
{
    QWriteLocker locker(&_mutex);
    _excludeFiles.append(path);
}

bool ExcludedFiles::reloadExcludes()
{
    QWriteLocker locker(&_mutex);
    c_strlist_destroy(*_excludesPtr);
    *_excludesPtr = NULL;

    bool success = true;
    foreach (const QString& file, _excludeFiles) {
        if (csync_exclude_load(file.toUtf8(), _excludesPtr) < 0)
            success = false;
    }
    return success;
}

CSYNC_EXCLUDE_TYPE ExcludedFiles::isExcluded(
        const QString& fullPath,
        const QString& relativePath,
        bool excludeHidden) const
{
    QFileInfo fi(fullPath);

    if( excludeHidden ) {
        if( fi.isHidden() || fi.fileName().startsWith(QLatin1Char('.')) ) {
            return CSYNC_FILE_EXCLUDE_HIDDEN;
        }
    }

    csync_ftw_type_e type = CSYNC_FTW_TYPE_FILE;
    if (fi.isDir()) {
        type = CSYNC_FTW_TYPE_DIR;
    }
    QReadLocker lock(&_mutex);
    return csync_excluded_no_ctx(*_excludesPtr, relativePath.toUtf8(), type);
}
