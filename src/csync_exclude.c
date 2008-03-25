/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008      by Andreas Schneider <mail@cynapses.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * vim: ts=2 sw=2 et cindent
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <unistd.h>

#include "c_lib.h"

#include "csync_private.h"
#include "csync_exclude.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.exclude"
#include "csync_log.h"

static void csync_exclude_add(CSYNC *ctx, const char *string) {
  if (ctx->excludes == NULL) {
    ctx->excludes = c_strlist_new(32);
  }

  if (ctx->excludes->count == ctx->excludes->size) {
    ctx->excludes = c_strlist_expand(ctx->excludes, 2 * ctx->excludes->size);
  }

  c_strlist_add(ctx->excludes, string);
}

int csync_exclude_load(CSYNC *ctx, const char *fname) {
  int fd = -1;
  int i, rc;
  off_t size;
  char *buf, *entry;

  fd = open(fname, O_RDONLY);
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
  buf = c_malloc(size);
  if (read(fd, buf, size) != size) {
    rc = -1;
    goto out;
  }
  close(fd);

  entry = buf;
  for (i = 0; i < size; i++) {
    if (buf[i] == '\n') {
      if (entry != buf + i) {
        buf[i] = '\0';
        if (*entry != '#' || *entry == '\n') {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Adding entry: %s", entry);
          printf("Adding entry: %s\n", entry);
          csync_exclude_add(ctx, entry);
        }
      }
      entry = buf + i + 1;
    }
  }
  SAFE_FREE(buf);

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

  if (ctx->excludes->count) {
    for (i = 0; i < ctx->excludes->count; i++) {
      if (fnmatch(ctx->excludes->vector[i], path, 0) == 0) {
        return 1;
      }
    }
  }
  return 0;
}

