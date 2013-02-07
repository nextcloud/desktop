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
 */

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "c_lib.h"
#include "c_jhash.h"

#include "csync_private.h"
#include "csync_exclude.h"
#include "csync_statedb.h"
#include "csync_update.h"
#include "csync_util.h"

#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.updater"
#include "csync_log.h"

static int _csync_detect_update(CSYNC *ctx, const char *file,
    const csync_vio_file_stat_t *fs, const int type) {
  uint64_t h = 0;
  size_t len = 0;
  size_t size = 0;
  const char *path = NULL;
  csync_file_stat_t *st = NULL;
  csync_file_stat_t *tmp = NULL;

  if ((file == NULL) || (fs == NULL)) {
    errno = EINVAL;
    return -1;
  }

  path = file;
  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (strlen(path) <= strlen(ctx->local.uri)) {
        return -1;
      }
      path += strlen(ctx->local.uri) + 1;
      break;
    case REMOTE_REPLICA:
      if (strlen(path) <= strlen(ctx->remote.uri)) {
        return -1;
      }
      path += strlen(ctx->remote.uri) + 1;
      break;
    default:
      path = NULL;
      return -1;
      break;
  }
  len = strlen(path);

  h = c_jhash64((uint8_t *) path, len, 0);
  size = sizeof(csync_file_stat_t) + len + 1;

  st = c_malloc(size);
  if (st == NULL) {
    return -1;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s - hash %llu, st size: %zu",
      path, (long long unsigned int) h, size);

  /* Set instruction by default to none */
  st->instruction = CSYNC_INSTRUCTION_NONE;

  /* check hardlink count */
  if (type == CSYNC_FTW_TYPE_FILE && fs->nlink > 1) {
    st->instruction = CSYNC_INSTRUCTION_IGNORE;
    goto out;
  }

  /* Update detection */
  if (csync_get_statedb_exists(ctx)) {
    tmp = csync_statedb_get_stat_by_hash(ctx, h);
    if (tmp && tmp->phash == h) {
      /* we have an update! */
      if (fs->mtime > tmp->modtime) {
        st->instruction = CSYNC_INSTRUCTION_EVAL;
        goto out;
      }
      st->instruction = CSYNC_INSTRUCTION_NONE;
    } else {
      /* check if the file has been renamed */
      if (ctx->current == LOCAL_REPLICA) {
        free(tmp);
        tmp = csync_statedb_get_stat_by_inode(ctx, fs->inode);
        if (tmp && tmp->inode == fs->inode) {
          /* inode found so the file has been renamed */
          st->instruction = CSYNC_INSTRUCTION_RENAME;
          goto out;
        } else {
          /* file not found in statedb */
          st->instruction = CSYNC_INSTRUCTION_NEW;
          goto out;
        }
      }
      /* remote and file not found in statedb */
      st->instruction = CSYNC_INSTRUCTION_NEW;
    }
  } else  {
    st->instruction = CSYNC_INSTRUCTION_NEW;
  }

out:
  SAFE_FREE(tmp);
  st->inode = fs->inode;
  st->mode = fs->mode;
  st->size = fs->size;
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
    case REMOTE_REPLICA:
      if (c_rbtree_insert(ctx->remote.tree, (void *) st) < 0) {
        SAFE_FREE(st);
        return -1;
      }
      break;
    default:
      break;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "file: %s, instruction: %s", st->path,
      csync_instruction_str(st->instruction));

  return 0;
}

int csync_walker(CSYNC *ctx, const char *file, const csync_vio_file_stat_t *fs,
    enum csync_ftw_flags_e flag) {
  switch (flag) {
    case CSYNC_FTW_FLAG_FILE:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s", file);

      return _csync_detect_update(ctx, file, fs, CSYNC_FTW_TYPE_FILE);
      break;
    case CSYNC_FTW_FLAG_SLINK:
      /* FIXME: implement support for symlinks, see csync_propagate.c too */
#if 0
      if (ctx->options.sync_symbolic_links) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "symlink: %s", file);

        return _csync_detect_update(ctx, file, fs, CSYNC_FTW_TYPE_SLINK);
      }
#endif
      break;
    case CSYNC_FTW_FLAG_DIR: /* enter directory */
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "directory: %s", file);

      return _csync_detect_update(ctx, file, fs, CSYNC_FTW_TYPE_DIR);
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
int csync_ftw(CSYNC *ctx, const char *uri, csync_walker_fn fn,
    unsigned int depth) {
  char errbuf[256] = {0};
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
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
          "opendir failed for %s - %s",
          uri,
          errbuf);
      goto error;
    }
  }

  while ((dirent = csync_vio_readdir(ctx, dh))) {
    const char *path = NULL;
    int flag;

    d_name = dirent->name;
    if (d_name == NULL) {
      goto error;
    }

    /* skip "." and ".." */
    if (d_name[0] == '.' && (d_name[1] == '\0'
          || (d_name[1] == '.' && d_name[2] == '\0'))) {
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      continue;
    }

    if (asprintf(&filename, "%s/%s", uri, d_name) < 0) {
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      goto error;
    }

    /* Create relative path for checking the exclude list */
    switch (ctx->current) {
      case LOCAL_REPLICA:
        path = filename + strlen(ctx->local.uri) + 1;
        break;
      case REMOTE_REPLICA:
        path = filename + strlen(ctx->remote.uri) + 1;
        break;
      default:
        break;
    }

    /* Check if file is excluded */
    if (csync_excluded(ctx, path)) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "%s excluded", path);
      SAFE_FREE(filename);
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      continue;
    }

    fs = csync_vio_file_stat_new();
    if (csync_vio_stat(ctx, filename, fs) == 0) {
      switch (fs->type) {
        case CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK:
          flag = CSYNC_FTW_FLAG_SLINK;
          break;
        case CSYNC_VIO_FILE_TYPE_DIRECTORY:
          flag = CSYNC_FTW_FLAG_DIR;
          break;
        case CSYNC_VIO_FILE_TYPE_BLOCK_DEVICE:
        case CSYNC_VIO_FILE_TYPE_CHARACTER_DEVICE:
        case CSYNC_VIO_FILE_TYPE_SOCKET:
          flag = CSYNC_FTW_FLAG_SPEC;
          break;
        case CSYNC_VIO_FILE_TYPE_FIFO:
          flag = CSYNC_FTW_FLAG_SPEC;
          break;
        default:
          flag = CSYNC_FTW_FLAG_FILE;
          break;
      };
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
    dirent = NULL;
  }
  csync_vio_closedir(ctx, dh);

done:
  csync_vio_file_stat_destroy(dirent);
  SAFE_FREE(filename);
  return rc;
error:
  if (dh != NULL) {
    csync_vio_closedir(ctx, dh);
  }
  SAFE_FREE(filename);
  return -1;
}

/* vim: set ts=8 sw=2 et cindent: */
