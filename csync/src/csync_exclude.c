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
#include <unistd.h>

#include "c_lib.h"
#include "c_private.h"

#include "csync_private.h"
#include "csync_exclude.h"
#include "csync_misc.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.exclude"
#include "csync_log.h"

#ifndef NDEBUG
static
#endif
int _csync_exclude_add(c_strlist_t **inList, const char *string) {
    return c_strlist_add_grow(inList, string);
}

int csync_exclude_load(const char *fname, c_strlist_t **list) {
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
  buf = c_malloc(size + 1);
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
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Adding entry: %s", entry);
          rc = _csync_exclude_add(list, entry);
          if (rc < 0) {
              goto out;
          }
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

void csync_exclude_clear(CSYNC *ctx) {
  c_strlist_clear(ctx->excludes);
}

void csync_exclude_destroy(CSYNC *ctx) {
  c_strlist_destroy(ctx->excludes);
}

CSYNC_EXCLUDE_TYPE csync_excluded(CSYNC *ctx, const char *path, int filetype) {

    CSYNC_EXCLUDE_TYPE match = CSYNC_NOT_EXCLUDED;

    match = csync_excluded_no_ctx( ctx->excludes, path, filetype );

    return match;
}

// See http://support.microsoft.com/kb/74496 and
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
// Additionally, we ignore '$Recycle.Bin', see https://github.com/owncloud/client/issues/2955
static const char* win_reserved_words[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
                                           "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4",
                                           "LPT5", "LPT6", "LPT7", "LPT8", "LPT9", "CLOCK$", "$Recycle.Bin" };

bool csync_is_windows_reserved_word(const char* filename) {

  size_t win_reserve_words_len = sizeof(win_reserved_words) / sizeof(char*);
  size_t j;

  for (j = 0; j < win_reserve_words_len; j++) {
    int len_reserved_word = strlen(win_reserved_words[j]);
    int len_filename = strlen(filename);
    if (len_filename == 2 && filename[1] == ':') {
        if (filename[0] >= 'a' && filename[0] <= 'z') {
            return true;
        }
        if (filename[0] >= 'A' && filename[0] <= 'Z') {
            return true;
        }
    }
    if (c_strncasecmp(filename, win_reserved_words[j], len_reserved_word) == 0) {
        if (len_filename == len_reserved_word) {
            return true;
        }
        if ((len_filename > len_reserved_word) && (filename[len_reserved_word] == '.')) {
            return true;
        }
    }
  }
  return false;
}

static CSYNC_EXCLUDE_TYPE _csync_excluded_common(c_strlist_t *excludes, const char *path, int filetype, bool check_leading_dirs) {
    size_t i = 0;
    const char *p = NULL;
    const char *bname = NULL;
    size_t blen = 0;
    char *conflict = NULL;
    int rc = -1;
    CSYNC_EXCLUDE_TYPE match = CSYNC_NOT_EXCLUDED;
    CSYNC_EXCLUDE_TYPE type  = CSYNC_NOT_EXCLUDED;

      for (p = path; *p; p++) {
        switch (*p) {
          case '\\':
          case ':':
          case '?':
          case '*':
          case '"':
          case '>':
          case '<':
          case '|':
            return CSYNC_FILE_EXCLUDE_INVALID_CHAR;
          default:
            break;
        }
      }

    /* split up the path */
    bname = strrchr(path, '/');
    if (bname) {
        bname += 1; // don't include the /
    } else {
        bname = path;
    }
    blen = strlen(bname);

    rc = csync_fnmatch(".csync_journal.db*", bname, 0);
    if (rc == 0) {
        match = CSYNC_FILE_SILENTLY_EXCLUDED;
        goto out;
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
    if (blen > 1 && (bname[blen-1]== ' ' || bname[blen-1]== '.' )) {
        match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
        goto out;
    }

    if (csync_is_windows_reserved_word(bname)) {
      match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
      goto out;
    }
  #endif

    rc = csync_fnmatch(".owncloudsync.log*", bname, 0);
    if (rc == 0) {
        match = CSYNC_FILE_SILENTLY_EXCLUDED;
        goto out;
    }

    /* Always ignore conflict files, not only via the exclude list */
    rc = csync_fnmatch("*_conflict-*", bname, 0);
    if (rc == 0) {
        match = CSYNC_FILE_SILENTLY_EXCLUDED;
        goto out;
    }

    if (getenv("CSYNC_CONFLICT_FILE_USERNAME")) {
        rc = asprintf(&conflict, "*_conflict_%s-*", getenv("CSYNC_CONFLICT_FILE_USERNAME"));
        if (rc < 0) {
            goto out;
        }
        rc = csync_fnmatch(conflict, path, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            SAFE_FREE(conflict);
            goto out;
        }
        SAFE_FREE(conflict);
    }

    if( ! excludes ) {
        goto out;
    }

    /* Build a list of path components to check. */
    c_strlist_t *path_components = c_strlist_new(32);
    char *path_split = strdup(path);
    size_t len = strlen(path_split);
    for (int j = len; ; --j) {
        // read backwards until a path separator is found
        if (j != 0 && path_split[j-1] != '/') {
            continue;
        }

        // check 'basename', i.e. for "/foo/bar/fi" we'd check 'fi', 'bar', 'foo'
        if (path_split[j] != 0) {
            c_strlist_add_grow(&path_components, path_split + j);
        }

        if (j == 0 || !check_leading_dirs) {
            break;
        }

        // check 'dirname', i.e. for "/foo/bar/fi" we'd check '/foo/bar', '/foo'
        path_split[j-1] = '\0';
        c_strlist_add_grow(&path_components, path_split);
    }
    SAFE_FREE(path_split);

    /* Loop over all exclude patterns and evaluate the given path */
    for (i = 0; match == CSYNC_NOT_EXCLUDED && i < excludes->count; i++) {
        bool match_dirs_only = false;
        char *pattern_stored = c_strdup(excludes->vector[i]);
        char* pattern = pattern_stored;

        type = CSYNC_FILE_EXCLUDE_LIST;
        if (strlen(pattern) < 1) {
            SAFE_FREE(pattern_stored);
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
        if (match == CSYNC_NOT_EXCLUDED) {
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
        }
        SAFE_FREE(pattern_stored);
    }
    c_strlist_destroy(path_components);

  out:

    return match;
}

CSYNC_EXCLUDE_TYPE csync_excluded_traversal(c_strlist_t *excludes, const char *path, int filetype) {
  return _csync_excluded_common(excludes, path, filetype, false);
}

CSYNC_EXCLUDE_TYPE csync_excluded_no_ctx(c_strlist_t *excludes, const char *path, int filetype) {
  return _csync_excluded_common(excludes, path, filetype, true);
}

