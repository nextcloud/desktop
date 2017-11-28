/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
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

#ifndef _CSYNC_EXCLUDE_H
#define _CSYNC_EXCLUDE_H

#include "ocsynclib.h"

#include "csync.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QRegularExpression>

enum csync_exclude_type_e {
  CSYNC_NOT_EXCLUDED   = 0,
  CSYNC_FILE_SILENTLY_EXCLUDED,
  CSYNC_FILE_EXCLUDE_AND_REMOVE,
  CSYNC_FILE_EXCLUDE_LIST,
  CSYNC_FILE_EXCLUDE_INVALID_CHAR,
  CSYNC_FILE_EXCLUDE_TRAILING_SPACE,
  CSYNC_FILE_EXCLUDE_LONG_FILENAME,
  CSYNC_FILE_EXCLUDE_HIDDEN,
  CSYNC_FILE_EXCLUDE_STAT_FAILED,
  CSYNC_FILE_EXCLUDE_CONFLICT,
  CSYNC_FILE_EXCLUDE_CANNOT_ENCODE
};
typedef enum csync_exclude_type_e CSYNC_EXCLUDE_TYPE;

/**
 * Manages file/directory exclusion.
 *
 * Most commonly exclude patterns are loaded from files. See
 * addExcludeFilePath() and reloadExcludeFiles().
 *
 * Excluded files are primarily relevant for sync runs, and for
 * file watcher filtering.
 *
 * Excluded files and ignored files are the same thing. But the
 * selective sync blacklist functionality is a different thing
 * entirely.
 */
class OCSYNC_EXPORT ExcludedFiles : public QObject
{
    Q_OBJECT
public:
    ExcludedFiles();
    ~ExcludedFiles();

    /**
     * Adds a new path to a file containing exclude patterns.
     *
     * Does not load the file. Use reloadExcludeFiles() afterwards.
     */
    void addExcludeFilePath(const QString &path);

    /**
     * Checks whether a file or directory should be excluded.
     *
     * @param filePath     the absolute path to the file
     * @param basePath     folder path from which to apply exclude rules, ends with a /
     */
    bool isExcluded(
        const QString &filePath,
        const QString &basePath,
        bool excludeHidden) const;

    /**
     * Adds an exclude pattern.
     *
     * Primarily used in tests. Patterns added this way are preserved when
     * reloadExcludeFiles() is called.
     */
    void addManualExclude(const QByteArray &expr);

    /**
     * Removes all manually added exclude patterns.
     *
     * Primarily used in tests.
     */
    void clearManualExcludes();

    /**
     * Generate a hook for traversal exclude pattern matching
     * that csync can use.
     *
     * Careful: The function will only be valid for as long as this
     * ExcludedFiles instance stays alive.
     */
    auto csyncTraversalMatchFun() const
        -> std::function<CSYNC_EXCLUDE_TYPE(const char *path, int filetype)>;

public slots:
    /**
     * Reloads the exclude patterns from the registered paths.
     */
    bool reloadExcludeFiles();

#ifdef CSYNC_TEST
public:
#else
private:
#endif
    /**
     * @brief Match the exclude pattern against the full path.
     *
     * @param Path is folder-relative, should not start with a /.
     *
     * Note that this only matches patterns. It does not check whether the file
     * or directory pointed to is hidden (or whether it even exists).
     */
    CSYNC_EXCLUDE_TYPE fullPatternMatch(const char *path, int filetype) const;

    /**
     * @brief Check if the given path should be excluded in a traversal situation.
     *
     * It does only part of the work that full() does because it's assumed
     * that all leading directories have been run through traversal()
     * before. This can be significantly faster.
     *
     * That means for 'foo/bar/file' only ('foo/bar/file', 'file') is checked
     * against the exclude patterns.
     *
     * @param Path is folder-relative, should not start with a /.
     *
     * Note that this only matches patterns. It does not check whether the file
     * or directory pointed to is hidden (or whether it even exists).
     */
    CSYNC_EXCLUDE_TYPE traversalPatternMatch(const char *path, int filetype) const;

    /**
     * Generate an optimized _regex for many of the patterns. The remaining
     * patterns are put into _nonRegexExcludes.
     */
    void prepare();

    /// Files to load excludes from
    QSet<QString> _excludeFiles;

    /// Exclude patterns added with addManualExclude()
    QList<QByteArray> _manualExcludes;

    /// List of all active exclude patterns
    QList<QByteArray> _allExcludes;

    /// see prepare()
    QList<QByteArray> _nonRegexExcludes;

    /// see prepare()
    QRegularExpression _bnameRegexFileDir;
    QRegularExpression _bnameRegexDir;
};

#endif /* _CSYNC_EXCLUDE_H */
