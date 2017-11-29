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

#include "config_csync.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "c_lib.h"
#include "c_private.h"
#include "c_utf8.h"

#include "csync_private.h"
#include "csync_exclude.h"
#include "csync_misc.h"

#include "common/utility.h"

#include <QString>
#include <QFileInfo>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define CSYNC_LOG_CATEGORY_NAME "csync.exclude"
#include "csync_log.h"

#ifndef WITH_TESTING
static
#endif

/** Expands C-like escape sequences.
 *
 * The returned string is heap-allocated and owned by the caller.
 */
static const char *csync_exclude_expand_escapes(const char * input)
{
    size_t i_len = strlen(input) + 1;
    char *out = (char*)c_malloc(i_len); // out can only be shorter

    size_t i = 0;
    size_t o = 0;
    for (; i < i_len; ++i) {
        if (input[i] == '\\') {
            // at worst input[i+1] is \0
            switch (input[i+1]) {
            case '\'': out[o++] = '\''; break;
            case '"': out[o++] = '"'; break;
            case '?': out[o++] = '?'; break;
            case '\\': out[o++] = '\\'; break;
            case '#': out[o++] = '#'; break;
            case 'a': out[o++] = '\a'; break;
            case 'b': out[o++] = '\b'; break;
            case 'f': out[o++] = '\f'; break;
            case 'n': out[o++] = '\n'; break;
            case 'r': out[o++] = '\r'; break;
            case 't': out[o++] = '\t'; break;
            case 'v': out[o++] = '\v'; break;
            default:
                out[o++] = input[i];
                out[o++] = input[i+1];
                break;
            }
            ++i;
        } else {
            out[o++] = input[i];
        }
    }
    return out;
}

/** Loads patterns from a file and adds them to excludes */
int csync_exclude_load(const char *fname, QList<QByteArray> *excludes) {
  int fd = -1;
  int i = 0;
  int rc = -1;
  int64_t size;
  char *buf = NULL;
  char *entry = NULL;
  mbchar_t *w_fname;

  if (fname == NULL) {
      return -1;
  }

#ifdef _WIN32
  _fmode = _O_BINARY;
#endif

  w_fname = c_utf8_path_to_locale(fname);
  if (w_fname == NULL) {
      return -1;
  }

  fd = _topen(w_fname, O_RDONLY);
  c_free_locale_string(w_fname);
  if (fd < 0) {
    return -1;
  }

  size = lseek(fd, 0, SEEK_END);
  if (size < 0) {
    rc = -1;
    goto out;
  }
  lseek(fd, 0, SEEK_SET);
  if (size == 0) {
    rc = 0;
    goto out;
  }
  buf = (char*)c_malloc(size + 1);
  if (read(fd, buf, size) != size) {
    rc = -1;
    goto out;
  }
  buf[size] = '\0';

  /* FIXME: Use fgets and don't add duplicates */
  entry = buf;
  for (i = 0; i < size; i++) {
    if (buf[i] == '\n' || buf[i] == '\r') {
      if (entry != buf + i) {
        buf[i] = '\0';
        if (*entry != '#') {
          const char *unescaped = csync_exclude_expand_escapes(entry);
          excludes->append(unescaped);
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Adding entry: %s", unescaped);
          SAFE_FREE(unescaped);
        }
      }
      entry = buf + i + 1;
    }
  }

  rc = 0;
out:
  SAFE_FREE(buf);
  close(fd);
  return rc;
}

// See http://support.microsoft.com/kb/74496 and
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
// Additionally, we ignore '$Recycle.Bin', see https://github.com/owncloud/client/issues/2955
static const char *win_reserved_words_3[] = { "CON", "PRN", "AUX", "NUL" };
static const char *win_reserved_words_4[] = {
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
};
static const char *win_reserved_words_n[] = { "CLOCK$", "$Recycle.Bin" };

/**
 * @brief Checks if filename is considered reserved by Windows
 * @param file_name filename
 * @return true if file is reserved, false otherwise
 */
bool csync_is_windows_reserved_word(const char *filename)
{
    size_t len_filename = strlen(filename);

    // Drive letters
    if (len_filename == 2 && filename[1] == ':') {
        if (filename[0] >= 'a' && filename[0] <= 'z') {
            return true;
        }
        if (filename[0] >= 'A' && filename[0] <= 'Z') {
            return true;
        }
    }

    if (len_filename == 3 || (len_filename > 3 && filename[3] == '.')) {
        for (const char *word : win_reserved_words_3) {
            if (c_strncasecmp(filename, word, 3) == 0) {
                return true;
            }
        }
    }

    if (len_filename == 4 || (len_filename > 4 && filename[4] == '.')) {
        for (const char *word : win_reserved_words_4) {
            if (c_strncasecmp(filename, word, 4) == 0) {
                return true;
            }
        }
    }

    for (const char *word : win_reserved_words_n) {
        size_t len_word = strlen(word);
        if (len_word == len_filename && c_strncasecmp(filename, word, len_word) == 0) {
            return true;
        }
    }

    return false;
}

static CSYNC_EXCLUDE_TYPE _csync_excluded_common(const QList<QByteArray> &excludes, const char *path, int filetype, bool check_leading_dirs)
{
    size_t i = 0;
    const char *bname = NULL;
    size_t blen = 0;
    int rc = -1;
    CSYNC_EXCLUDE_TYPE match = CSYNC_NOT_EXCLUDED;
    CSYNC_EXCLUDE_TYPE type  = CSYNC_NOT_EXCLUDED;
    c_strlist_t *path_components = NULL;

    /* split up the path */
    bname = strrchr(path, '/');
    if (bname) {
        bname += 1; // don't include the /
    } else {
        bname = path;
    }
    blen = strlen(bname);

    // 9 = strlen(".sync_.db")
    if (blen >= 9 && bname[0] == '.') {
        rc = csync_fnmatch("._sync_*.db*", bname, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
        rc = csync_fnmatch(".sync_*.db*", bname, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
        rc = csync_fnmatch(".csync_journal.db*", bname, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
        rc = csync_fnmatch(".owncloudsync.log*", bname, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
    }

    // check the strlen and ignore the file if its name is longer than 254 chars.
    // whenever changing this also check createDownloadTmpFileName
    if (blen > 254) {
        match = CSYNC_FILE_EXCLUDE_LONG_FILENAME;
        goto out;
    }

#ifdef _WIN32
    // Windows cannot sync files ending in spaces (#2176). It also cannot
    // distinguish files ending in '.' from files without an ending,
    // as '.' is a separator that is not stored internally, so let's
    // not allow to sync those to avoid file loss/ambiguities (#416)
    if (blen > 1) {
        if (bname[blen-1]== ' ') {
            match = CSYNC_FILE_EXCLUDE_TRAILING_SPACE;
            goto out;
        } else if (bname[blen-1]== '.' ) {
            match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
            goto out;
        }
    }

    if (csync_is_windows_reserved_word(bname)) {
      match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
      goto out;
    }

    // Filter out characters not allowed in a filename on windows
    for (const char *p = path; *p; p++) {
        switch (*p) {
        case '\\':
        case ':':
        case '?':
        case '*':
        case '"':
        case '>':
        case '<':
        case '|':
            match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
            goto out;
        default:
            break;
        }
    }
#endif

    /* We create a desktop.ini on Windows for the sidebar icon, make sure we don't sync them. */
    rc = csync_fnmatch("Desktop.ini", bname, 0);
    if (rc == 0) {
        match = CSYNC_FILE_SILENTLY_EXCLUDED;
        goto out;
    }

    if (!OCC::Utility::shouldUploadConflictFiles()) {
        if (OCC::Utility::isConflictFile(bname)) {
            match = CSYNC_FILE_EXCLUDE_CONFLICT;
            goto out;
        }
    }

    if (excludes.isEmpty()) {
        goto out;
    }

    if (check_leading_dirs) {
        /* Build a list of path components to check. */
        path_components = c_strlist_new(32);
        char *path_split = strdup(path);
        size_t len = strlen(path_split);
        for (i = len; ; --i) {
            // read backwards until a path separator is found
            if (i != 0 && path_split[i-1] != '/') {
                continue;
            }

            // check 'basename', i.e. for "/foo/bar/fi" we'd check 'fi', 'bar', 'foo'
            if (path_split[i] != 0) {
                c_strlist_add_grow(&path_components, path_split + i);
            }

            if (i == 0) {
                break;
            }

            // check 'dirname', i.e. for "/foo/bar/fi" we'd check '/foo/bar', '/foo'
            path_split[i-1] = '\0';
            c_strlist_add_grow(&path_components, path_split);
        }
        SAFE_FREE(path_split);
    }

    /* Loop over all exclude patterns and evaluate the given path */
    for (const auto &exclude : excludes) {
        if (match != CSYNC_NOT_EXCLUDED)
            break;

        bool match_dirs_only = false;

        // If the pattern ends with a slash, *it will be temporarily modified
        // inside this function* and then reverted. This is left over from
        // old code and will be removed.
        char *pattern = const_cast<char *>(exclude.data());

        type = CSYNC_FILE_EXCLUDE_LIST;
        if (!pattern[0]) { /* empty pattern */
            continue;
        }
        /* Excludes starting with ']' means it can be cleanup */
        if (pattern[0] == ']') {
            ++pattern;
            if (filetype == CSYNC_FTW_TYPE_FILE) {
                type = CSYNC_FILE_EXCLUDE_AND_REMOVE;
            }
        }
        /* Check if the pattern applies to pathes only. */
        if (pattern[strlen(pattern)-1] == '/') {
            if (!check_leading_dirs && filetype == CSYNC_FTW_TYPE_FILE) {
                continue;
            }
            match_dirs_only = true;
            pattern[strlen(pattern)-1] = '\0'; /* Cut off the slash */
        }

        /* check if the pattern contains a / and if, compare to the whole path */
        if (strchr(pattern, '/')) {
            rc = csync_fnmatch(pattern, path, FNM_PATHNAME);
            if( rc == 0 ) {
                match = type;
            }
            /* if the pattern requires a dir, but path is not, its still not excluded. */
            if (match_dirs_only && filetype != CSYNC_FTW_TYPE_DIR) {
                match = CSYNC_NOT_EXCLUDED;
            }
        }

        /* if still not excluded, check each component and leading directory of the path */
        if (match == CSYNC_NOT_EXCLUDED && check_leading_dirs) {
            size_t j = 0;
            if (match_dirs_only && filetype == CSYNC_FTW_TYPE_FILE) {
                j = 1; // skip the first entry, which is bname
            }
            for (; j < path_components->count; ++j) {
                rc = csync_fnmatch(pattern, path_components->vector[j], 0);
                if (rc == 0) {
                    match = type;
                    break;
                }
            }
        } else if (match == CSYNC_NOT_EXCLUDED && !check_leading_dirs) {
            rc = csync_fnmatch(pattern, bname, 0);
            if (rc == 0) {
                match = type;
            }
        }
        if (match_dirs_only) {
            /* restore the '/' */
            pattern[strlen(pattern)] = '/';
        }
    }
    c_strlist_destroy(path_components);

  out:

    return match;
}


using namespace OCC;

ExcludedFiles::ExcludedFiles()
{
}

ExcludedFiles::~ExcludedFiles()
{
}

void ExcludedFiles::addExcludeFilePath(const QString &path)
{
    _excludeFiles.insert(path);
}

void ExcludedFiles::addManualExclude(const QByteArray &expr)
{
    _manualExcludes.append(expr);
    _allExcludes.append(expr);
    _nonRegexExcludes.append(expr);
}

void ExcludedFiles::clearManualExcludes()
{
    _manualExcludes.clear();
    reloadExcludeFiles();
}

bool ExcludedFiles::reloadExcludeFiles()
{
    _allExcludes.clear();
    _regex = QRegularExpression();

    bool success = true;
    foreach (const QString &file, _excludeFiles) {
        if (csync_exclude_load(file.toUtf8(), &_allExcludes) < 0)
            success = false;
    }

    _allExcludes.append(_manualExcludes);

    prepare();

    return success;
}

bool ExcludedFiles::isExcluded(
    const QString &filePath,
    const QString &basePath,
    bool excludeHidden) const
{
    if (!filePath.startsWith(basePath, Utility::fsCasePreserving() ? Qt::CaseInsensitive : Qt::CaseSensitive)) {
        // Mark paths we're not responsible for as excluded...
        return true;
    }

    if (excludeHidden) {
        QString path = filePath;
        // Check all path subcomponents, but to *not* check the base path:
        // We do want to be able to sync with a hidden folder as the target.
        while (path.size() > basePath.size()) {
            QFileInfo fi(path);
            if (fi.isHidden() || fi.fileName().startsWith(QLatin1Char('.'))) {
                return true;
            }

            // Get the parent path
            path = fi.absolutePath();
        }
    }

    QFileInfo fi(filePath);
    csync_ftw_type_e type = CSYNC_FTW_TYPE_FILE;
    if (fi.isDir()) {
        type = CSYNC_FTW_TYPE_DIR;
    }

    QString relativePath = filePath.mid(basePath.size());
    if (relativePath.endsWith(QLatin1Char('/'))) {
        relativePath.chop(1);
    }

    return fullPatternMatch(relativePath.toUtf8(), type) != CSYNC_NOT_EXCLUDED;
}

CSYNC_EXCLUDE_TYPE ExcludedFiles::traversalPatternMatch(const char *path, int filetype) const
{
    CSYNC_EXCLUDE_TYPE match = CSYNC_NOT_EXCLUDED;

    /* Check only static patterns and only with the reduced list which is empty usually */
    match = _csync_excluded_common(_nonRegexExcludes, path, filetype, false);
    if (match != CSYNC_NOT_EXCLUDED) {
        return match;
    }

    if (!_allExcludes.isEmpty()) {
        /* Now check with our optimized regexps */
        const char *bname = NULL;
        /* split up the path */
        bname = strrchr(path, '/');
        if (bname) {
            bname += 1; // don't include the /
        } else {
            bname = path;
        }
        QString p = QString::fromUtf8(bname);
        auto m = _regex.match(p);
        if (m.hasMatch()) {
            if (!m.captured(1).isEmpty()) {
                match = CSYNC_FILE_EXCLUDE_LIST;
            } else if (!m.captured(2).isEmpty()) {
                match = CSYNC_FILE_EXCLUDE_AND_REMOVE;
            }
        }
    }
    return match;
}

CSYNC_EXCLUDE_TYPE ExcludedFiles::fullPatternMatch(const char *path, int filetype) const
{
    return _csync_excluded_common(_allExcludes, path, filetype, true);
}

auto ExcludedFiles::csyncTraversalMatchFun() const
    -> std::function<CSYNC_EXCLUDE_TYPE(const char *path, int filetype)>
{
    return [this](const char *path, int filetype) { return this->traversalPatternMatch(path, filetype); };
}

/* Only for bnames (not paths) */
static QString convertToBnameRegexpSyntax(QString exclude)
{
    QString s = QRegularExpression::escape(exclude).replace("\\*", ".*").replace("\\?", ".");
    return s;
}

void ExcludedFiles::prepare()
{
    _nonRegexExcludes.clear();

    // Start out with regexes that would match nothing
    QString exclude_only = "a^";
    QString exclude_and_remove = "a^";

    for (auto exclude : _allExcludes) {
        QString *builderToUse = &exclude_only;
        if (exclude[0] == '\n')
            continue; // empty line
        if (exclude[0] == '\r')
            continue; // empty line

        /* If an exclude entry contains some fnmatch-ish characters, we use the C-style codepath without QRegularEpression */
        if (strchr(exclude, '/') || strchr(exclude, '[') || strchr(exclude, '{') || strchr(exclude, '\\')) {
            _nonRegexExcludes.append(exclude);
            continue;
        }

        /* Those will attempt to use QRegularExpression */
        if (exclude[0] == ']') {
            exclude = exclude.mid(1);
            builderToUse = &exclude_and_remove;
        }
        if (builderToUse->size() > 0) {
            builderToUse->append("|");
        }
        builderToUse->append(convertToBnameRegexpSyntax(QString::fromUtf8(exclude)));
    }

    QString pattern = "^(" + exclude_only + ")$|^(" + exclude_and_remove + ")$";
    _regex.setPattern(pattern);
    QRegularExpression::PatternOptions patternOptions = QRegularExpression::OptimizeOnFirstUsageOption;
    if (OCC::Utility::fsCasePreserving())
        patternOptions |= QRegularExpression::CaseInsensitiveOption;
    _regex.setPatternOptions(patternOptions);
    _regex.optimize();
}

