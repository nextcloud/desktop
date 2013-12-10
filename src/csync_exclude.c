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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "c_lib.h"

#include "csync_private.h"
#include "csync_exclude.h"
#include "csync_misc.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.exclude"
#include "csync_log.h"

static int _csync_exclude_add(CSYNC *ctx, const char *string) {
    c_strlist_t *list;

    if (ctx->excludes == NULL) {
        ctx->excludes = c_strlist_new(32);
        if (ctx->excludes == NULL) {
            return -1;
        }
    }

    if (ctx->excludes->count == ctx->excludes->size) {
        list = c_strlist_expand(ctx->excludes, 2 * ctx->excludes->size);
        if (list == NULL) {
            return -1;
        }
        ctx->excludes = list;
    }

    return c_strlist_add(ctx->excludes, string);
}

int csync_exclude_load(CSYNC *ctx, const char *fname) {
  int fd = -1;
  int i = 0;
  int rc = -1;
  int64_t size;
  char *buf = NULL;
  char *entry = NULL;
  mbchar_t *w_fname;

  if (ctx == NULL || fname == NULL) {
      return -1;
  }

#ifdef _WIN32
  _fmode = _O_BINARY;
#endif

  w_fname = c_utf8_to_locale(fname);
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
  if (buf == NULL) {
      rc = -1;
      goto out;
  }

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
          rc = _csync_exclude_add(ctx, entry);
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
  size_t i = 0;
  const char *p = NULL;
  char *bname = NULL;
  char *dname = NULL;
  char *prev_dname = NULL;
  int rc = -1;
  CSYNC_EXCLUDE_TYPE match = CSYNC_NOT_EXCLUDED;
  CSYNC_EXCLUDE_TYPE type  = CSYNC_NOT_EXCLUDED;

  /* exclude the lock file */
  if (c_streq( path, CSYNC_LOCK_FILE )) {
      return CSYNC_FILE_SILENTLY_EXCLUDED;
  }

  if (! ctx->options.unix_extensions) {
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
  }

  /* split up the path */
  dname = c_dirname(path);
  bname = c_basename(path);

  if (bname == NULL || dname == NULL) {
      match = CSYNC_NOT_EXCLUDED;
      SAFE_FREE(bname);
      SAFE_FREE(dname);
      goto out;
  }

  rc = csync_fnmatch(".csync_journal.db*", bname, 0);
  if (rc == 0) {
      match = CSYNC_FILE_SILENTLY_EXCLUDED;
      SAFE_FREE(bname);
      SAFE_FREE(dname);
      goto out;
  }

  SAFE_FREE(bname);
  SAFE_FREE(dname);

  SAFE_FREE(bname);
  SAFE_FREE(dname);

  if (ctx->excludes == NULL) {
      goto out;
  }

  /* Loop over all exclude patterns and evaluate the given path */
  for (i = 0; match == CSYNC_NOT_EXCLUDED && i < ctx->excludes->count; i++) {
      bool match_dirs_only = false;
      char *pattern_stored = c_strdup(ctx->excludes->vector[i]);
      char* pattern = pattern_stored;

      type = CSYNC_FILE_EXCLUDE_LIST;
      if (strlen(pattern) < 1) {
          continue;
      }
      /* Ecludes starting with ']' means it can be cleanup */
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

      /* if still not excluded, check each component of the path */
      if (match == CSYNC_NOT_EXCLUDED) {
          int trailing_component = 1;
          dname = c_dirname(path);
          bname = c_basename(path);

          if (bname == NULL || dname == NULL) {
              match = CSYNC_NOT_EXCLUDED;
              goto out;
          }

          /* Check each component of the path */
          do {
              /* Do not check the bname if its a file and the pattern matches dirs only. */
              if ( !(trailing_component == 1 /* it is the trailing component */
                     && match_dirs_only      /* but only directories are matched by the pattern */
                     && filetype == CSYNC_FTW_TYPE_FILE) ) {
                  /* Check the name component against the pattern */
                  rc = csync_fnmatch(pattern, bname, 0);
                  if (rc == 0) {
                      match = type;
                  }
              }
              if (!(c_streq(dname, ".") || c_streq(dname, "/"))) {
                  rc = csync_fnmatch(pattern, dname, 0);
                  if (rc == 0) {
                      match = type;
                  }
              }
              trailing_component = 0;
              prev_dname = dname;
              SAFE_FREE(bname);
              bname = c_basename(prev_dname);
              dname = c_dirname(prev_dname);
              SAFE_FREE(prev_dname);

          } while( match == CSYNC_NOT_EXCLUDED && !c_streq(dname, ".")
                     && !c_streq(dname, "/") );
      }
      SAFE_FREE(pattern_stored);
      SAFE_FREE(bname);
      SAFE_FREE(dname);
  }

out:

  return match;
}

