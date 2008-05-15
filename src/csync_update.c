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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>

#include "c_lib.h"
#include "c_jhash.h"

#include "csync_private.h"
#include "csync_exclude.h"
#include "csync_journal.h"
#include "csync_update.h"
#include "csync_util.h"

#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.updater"
#include "csync_log.h"

static int csync_detect_update(CSYNC *ctx, const char *file, const csync_vio_file_stat_t *fs, const int type) {
  uint64_t h;
  size_t len;
  const char *path = NULL;
  csync_file_stat_t *st = NULL;
  csync_file_stat_t *tmp = NULL;

  if ((file == NULL) || (fs == NULL)) {
    errno = EINVAL;
    return -1;
  }

  switch (ctx->current) {
    case LOCAL_REPLICA:
      path = file + strlen(ctx->local.uri) + 1;
      break;
    case REMOTE_REPLCIA:
      path = file + strlen(ctx->remote.uri) + 1;
      break;
    default:
      break;
  }
  if (path == NULL) {
    return -1;
  }
  len = strlen(path);

  /* Check if file is excluded */
  if (csync_excluded(ctx, path)) {
    return 0;
  }

  h = c_jhash64((uint8_t *) path, len, 0);

  st = c_malloc(sizeof(csync_file_stat_t) + len + 1);
  if (st == NULL) {
    return -1;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s - hash %lu, stat %d",
      path, h, sizeof(csync_file_stat_t) + len + 1);

  /* check hardlink count */
  if (type == CSYNC_FTW_TYPE_FILE && fs->nlink > 1) {
    st->instruction = CSYNC_INSTRUCTION_IGNORE;
    goto out;
  }

  /* Update detection */
  if (ctx->journal.exists) {
    tmp = csync_journal_get_stat_by_hash(ctx, h);
    if (tmp == NULL) {
      /* check if the file has been renamed */
      if (ctx->current == LOCAL_REPLICA) {
        tmp = csync_journal_get_stat_by_inode(ctx, fs->inode);
        if (tmp == NULL) {
          /* file not found in journal */
          st->instruction = CSYNC_INSTRUCTION_NEW;
          goto out;
        } else {
          /* inode found so the file has been renamed */
          st->instruction = CSYNC_INSTRUCTION_RENAME;
          goto out;
        }
      }
      /* remote and file not found in journal */
      st->instruction = CSYNC_INSTRUCTION_NEW;
      goto out;
    } else {
      /* we have an update! */
      if (fs->mtime > tmp->modtime) {
        st->instruction = CSYNC_INSTRUCTION_EVAL;
        goto out;
      }
      /* FIXME: check mode too? */
    }
    st->instruction = CSYNC_INSTRUCTION_NONE;
  } else  {
    st->instruction = CSYNC_INSTRUCTION_NEW;
    goto out;
  }

out:
  SAFE_FREE(tmp);
  st->inode = fs->inode;
  st->mode = fs->mode;
  st->modtime = fs->mtime;
  st->uid = fs->uid;
  st->gid = fs->gid;
  st->nlink = fs->nlink;
  st->type = type;

  st->phash = h;
  st->pathlen = len;
  memcpy(st->path, (len ? path : ""), len + 1);

  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (c_rbtree_insert(ctx->local.tree, (void *) st) < 0) {
        SAFE_FREE(st);
        return -1;
      }
      break;
    case REMOTE_REPLCIA:
      if (c_rbtree_insert(ctx->remote.tree, (void *) st) < 0) {
        SAFE_FREE(st);
        return -1;
      }
      break;
    default:
      break;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "file: %s, instruction: %s", st->path, csync_instruction_str(st->instruction));

  return 0;
}

int csync_walker(CSYNC *ctx, const char *file, const csync_vio_file_stat_t *fs, enum csync_ftw_flags_e flag) {
  switch (flag) {
    case CSYNC_FTW_FLAG_FILE:
    case CSYNC_FTW_FLAG_SLINK:
      switch (fs->mode & S_IFMT) {
        case S_IFREG:
        case S_IFLNK:
          /* TODO: handle symbolic links on unix systems */
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s", file);

          return csync_detect_update(ctx, file, fs, CSYNC_FTW_TYPE_FILE);
          break;
        default:
          break;
      }
    case CSYNC_FTW_FLAG_DIR: /* enter directory */
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "directory: %s", file);

      return csync_detect_update(ctx, file, fs, CSYNC_FTW_TYPE_DIR);
    case CSYNC_FTW_FLAG_NSTAT: /* not statable file */
    case CSYNC_FTW_FLAG_DNR:
    case CSYNC_FTW_FLAG_DP:
    case CSYNC_FTW_FLAG_SLN:
      break;
    default:
      break;
  }

  return 0;
}

/* File tree walker */
int csync_ftw(CSYNC *ctx, const char *uri, csync_walker_fn fn, unsigned int depth) {
  char *filename = NULL;
  char *d_name = NULL;
  csync_vio_handle_t *dh = NULL;
  csync_vio_file_stat_t *dirent = NULL;
  csync_vio_file_stat_t *fs = NULL;
  int rc = 0;

  if (uri[0] == '\0') {
    errno = ENOENT;
    goto error;
  }

  if ((dh = csync_vio_opendir(ctx, uri)) == NULL) {
    /* permission denied */
    if (errno == EACCES) {
      return 0;
    } else {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "opendir failed for %s - %s", uri, strerror(errno));
      goto error;
    }
  }

  while ((dirent = csync_vio_readdir(ctx, dh))) {
    int flag;

    d_name = dirent->name;
    if (d_name == NULL) {
      goto error;
    }

    /* skip "." and ".." */
    if (d_name[0] == '.' && (d_name[1] == '\0'
          || (d_name[1] == '.' && d_name[2] == '\0'))) {
      csync_vio_file_stat_destroy(dirent);
      continue;
    }

    if (asprintf(&filename, "%s/%s", uri, d_name) < 0) {
      csync_vio_file_stat_destroy(dirent);
      goto error;
    }

    fs = csync_vio_file_stat_new();
    if (csync_vio_stat(ctx, filename, fs) == 0) {
      if (fs->type == CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK) {
        flag = CSYNC_FTW_FLAG_SLINK;
      } else if (fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY) {
        flag = CSYNC_FTW_FLAG_DIR;
      } else {
        flag = CSYNC_FTW_FLAG_FILE;
      }
    } else {
      flag = CSYNC_FTW_FLAG_NSTAT;
    }

    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "walk: %s", filename);

    /* Call walker function for each file */
    rc = fn(ctx, filename, fs, flag);
    csync_vio_file_stat_destroy(fs);

    if (rc < 0) {
      csync_vio_closedir(ctx, dh);
      goto done;
    }

    if (flag == CSYNC_FTW_FLAG_DIR && depth) {
      rc = csync_ftw(ctx, filename, fn, depth - 1);
      if (rc < 0) {
        csync_vio_closedir(ctx, dh);
        goto done;
      }
    }
    SAFE_FREE(filename);
    csync_vio_file_stat_destroy(dirent);
  }
  csync_vio_closedir(ctx, dh);

done:
  csync_vio_file_stat_destroy(dirent);
  SAFE_FREE(filename);
  return rc;
error:
  SAFE_FREE(filename);
  return -1;
}

