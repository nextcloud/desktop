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
#include "utility.h"

#include <QFileInfo>

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
    _excludeFiles.insert(path);
}

bool ExcludedFiles::reloadExcludes()
{
    c_strlist_destroy(*_excludesPtr);
    *_excludesPtr = NULL;

    bool success = true;
    foreach (const QString& file, _excludeFiles) {
        if (csync_exclude_load(file.toUtf8(), _excludesPtr) < 0)
            success = false;
    }
    return success;
}

bool ExcludedFiles::isExcluded(
        const QString& filePath,
        const QString& basePath,
        bool excludeHidden) const
{
    if (!filePath.startsWith(basePath, Utility::fsCasePreserving() ? Qt::CaseInsensitive : Qt::CaseSensitive)) {
        // Mark paths we're not responsible for as excluded...
        return true;
    }

    QFileInfo fi(filePath);
    if( excludeHidden ) {
        if( fi.isHidden() || fi.fileName().startsWith(QLatin1Char('.')) ) {
            return true;
        }
    }

    csync_ftw_type_e type = CSYNC_FTW_TYPE_FILE;
    if (fi.isDir()) {
        type = CSYNC_FTW_TYPE_DIR;
    }

    QString relativePath = filePath.mid(basePath.size());
    if (relativePath.endsWith(QLatin1Char('/'))) {
        relativePath.chop(1);
    }

    return csync_excluded_no_ctx(*_excludesPtr, relativePath.toUtf8(), type) != CSYNC_NOT_EXCLUDED;
}
