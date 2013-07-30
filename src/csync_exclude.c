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
  off_t size;
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
    if (buf[i] == '\n') {
      if (entry != buf + i) {
        buf[i] = '\0';
        if (*entry != '#' || *entry == '\n') {
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

void csync_exclude_destroy(CSYNC *ctx) {
  c_strlist_destroy(ctx->excludes);
}

int csync_excluded(CSYNC *ctx, const char *path) {
  size_t i;
  const char *p;
  char *bname;
  int rc;
  int match = 0;

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
          return 1;
        default:
          break;
      }
    }
  }

  rc = csync_fnmatch(".csync_journal.db*", path, 0);
  if (rc == 0) {
      return 1;
  }

  bname = c_basename(path);
  if (bname == NULL) {
      return 0;
  }

  rc = csync_fnmatch(".csync_journal.db*", bname, 0);
  if (rc == 0) {
      match = 1;
      goto out;
  }

  if (ctx->excludes == NULL) {
      goto out;
  }

  for (i = 0; match == 0 && i < ctx->excludes->count; i++) {
      rc = csync_fnmatch(ctx->excludes->vector[i], path, 0);
      if (rc == 0) {
          match = 1;
      }

      rc = csync_fnmatch(ctx->excludes->vector[i], bname, 0);
      if (rc == 0) {
          match = 1;
      }
  }

out:
  free(bname);
  return match;
}

