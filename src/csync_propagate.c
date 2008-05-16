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
#include <string.h>

#include "csync_private.h"
#include "csync_propagate.h"
#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.propagator"
#include "csync_log.h"

static int csync_push_file(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e srep;
  enum csync_replica_e drep;
  enum csync_replica_e rep_bak;

  char *suri = NULL;
  char *duri = NULL;
  char *turi = NULL;
  char *tdir = NULL;

  csync_vio_handle_t *sfp = NULL;
  csync_vio_handle_t *dfp = NULL;

  csync_vio_file_stat_t *tstat = NULL;

  char buf[MAX_XFER_BUF_SIZE] = {0};
  ssize_t bread = 0;
  ssize_t bwritten = 0;
  struct timeval times[2];

  int rc = -1;

  rep_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      srep = ctx->local.type;
      drep = ctx->remote.type;
      if (asprintf(&suri, "%s/%s", ctx->local.uri, st->path) < 0) {
        rc = -1;
        goto out;
      }
      if (asprintf(&duri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        rc = -1;
        goto out;
      }
      break;
    case REMOTE_REPLCIA:
      srep = ctx->remote.type;
      drep = ctx->local.type;
      if (asprintf(&suri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        rc = -1;
        goto out;
      }
      if (asprintf(&duri, "%s/%s", ctx->local.uri, st->path) < 0) {
        rc = -1;
        goto out;
      }
      break;
    default:
      break;
  }

  if (asprintf(&turi, "%s.ctmp", duri) < 0) {
    rc = -1;
    goto out;
  }

  tdir = c_dirname(turi);
  if (tdir == NULL) {
    rc = -1;
    goto out;
  }

  /* Open the source file */
  ctx->replica = srep;
  sfp = csync_vio_open(ctx, suri, O_RDONLY, 0);
  if (sfp == NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, error: %s", suri, strerror(errno));
    rc = 1;
    goto out;
  }

  /* Open the destination file */
  ctx->replica = drep;
  csync_vio_mkdir(ctx, tdir, 0755);
  dfp = csync_vio_creat(ctx, turi, 0644);
  if (dfp == NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, error: %s", duri, strerror(errno));
    rc = 1;
    goto out;
  }

  /* copy file */
  for (;;) {
    ctx->replica = srep;
    bread = csync_vio_read(ctx, sfp, buf, MAX_XFER_BUF_SIZE);

    if (bread < 0) {
      /* read error */
      rc = 1;
      goto out;
    } else if (bread == 0) {
      /* done */
      break;
    }

    ctx->replica = drep;
    bwritten = csync_vio_write(ctx, dfp, buf, MAX_XFER_BUF_SIZE);

    if (bwritten < 0 || bread != bwritten) {
      rc = 1;
      goto out;
    }
  }

  /* check filesize */
  ctx->replica = drep;
  tstat = csync_vio_file_stat_new();
  if (csync_vio_stat(ctx, turi, tstat) < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, error: %s", duri, strerror(errno));
    rc = 1;
    goto out;
  }

  if (st->size != tstat->size) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s - error: incorrect filesize", turi);
    rc = 1;
    goto out;
  }

  /* override original file */
  if (csync_vio_rename(ctx, turi, duri) < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, error: %s", duri, strerror(errno));
    rc = 1;
    goto out;
  }

  /* sync time */
  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  csync_vio_utimes(ctx, duri, times);

  /* sync modes */
  csync_vio_chmod(ctx, duri, st->mode);

  st->instruction = CSYNC_INSTRUCTION_NONE;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "file: %s, instruction: PUSHED", duri);

  ctx->replica = rep_bak;

  rc = 0;

out:
  ctx->replica = srep;
  csync_vio_close(ctx, sfp);

  ctx->replica = drep;
  csync_vio_close(ctx, dfp);

  csync_vio_file_stat_destroy(tstat);

  SAFE_FREE(suri);
  SAFE_FREE(duri);
  SAFE_FREE(turi);
  SAFE_FREE(tdir);

  return rc;
}

static int csync_new_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;

  rc = csync_push_file(ctx, st);

  return rc;
}

static int csync_sync_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;

  rc = csync_push_file(ctx, st);

  return rc;
}

static int csync_remove_file(CSYNC *ctx, csync_file_stat_t *st) {
  char *uri = NULL;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLCIA:
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  csync_vio_unlink(ctx, uri);

  st->instruction = CSYNC_INSTRUCTION_NONE;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "file: %s, instruction: REMOVED", uri);

  return 0;
}

static int csync_new_dir(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e src;
  enum csync_replica_e dest;
  enum csync_replica_e replica_bak;
  char *path = NULL;
  struct timeval times[2];

  replica_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      src = ctx->local.type;
      dest = ctx->remote.type;
      if (asprintf(&path, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLCIA:
      src = ctx->remote.type;
      dest = ctx->local.type;
      if (asprintf(&path, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  /* TODO: check return values and errno */

  ctx->replica = dest;
  csync_vio_mkdirs(ctx, path, 0755);

  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  csync_vio_utimes(ctx, path, times);
  csync_vio_chmod(ctx, path, st->mode);

  st->instruction = CSYNC_INSTRUCTION_NONE;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "dir: %s, instruction: CREATED", path);
  ctx->replica = replica_bak;

  return 0;
}

static int csync_sync_dir(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e src;
  enum csync_replica_e dest;
  enum csync_replica_e replica_bak;
  char *path = NULL;
  struct timeval times[2];

  replica_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      src = ctx->local.type;
      dest = ctx->remote.type;
      if (asprintf(&path, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLCIA:
      src = ctx->remote.type;
      dest = ctx->local.type;
      if (asprintf(&path, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  ctx->replica = dest;

  /* TODO: check return values */

  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  csync_vio_utimes(ctx, path, times);
  csync_vio_chmod(ctx, path, st->mode);

  st->instruction = CSYNC_INSTRUCTION_NONE;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "dir: %s, instruction: SYNCED", path);
  ctx->replica = replica_bak;

  return 0;
}

static int csync_remove_dir(CSYNC *ctx, csync_file_stat_t *st) {
  char *path = NULL;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (asprintf(&path, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLCIA:
      if (asprintf(&path, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  csync_vio_rmdir(ctx, path);

  st->instruction = CSYNC_INSTRUCTION_NONE;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "dir: %s, instruction: REMOVED", path);

  return 0;
}

static int csync_propagation_visitor(void *obj, void *data) {
  csync_file_stat_t *st = NULL;
  CSYNC *ctx = NULL;

  st = (csync_file_stat_t *) obj;
  ctx = (CSYNC *) data;

  switch(st->type) {
    case CSYNC_FTW_TYPE_FILE:
      switch (st->instruction) {
        case CSYNC_INSTRUCTION_NEW:
          break;
        case CSYNC_INSTRUCTION_SYNC:
          break;
        case CSYNC_INSTRUCTION_REMOVE:
          break;
        default:
          break;
      }
      break;
    case CSYNC_FTW_TYPE_DIR:
      switch (st->instruction) {
        case CSYNC_INSTRUCTION_NEW:
          if (csync_new_dir(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_SYNC:
          if (csync_sync_dir(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_REMOVE:
          /*
           * We have to remove after the propagation,
           * when the files have been removed and the
           * dirs are emtpy.
           */
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

  return 0;
err:
  CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "file: %s, error: %s", st->path, strerror(errno));
  return -1;
}

int csync_propapate_files(CSYNC *ctx) {
  c_rbtree_t *tree = NULL;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      tree = ctx->local.tree;
      break;
    case REMOTE_REPLCIA:
      tree = ctx->remote.tree;
      break;
    default:
      break;
  }

  if (c_rbtree_walk(tree, (void *) ctx, csync_propagation_visitor) < 0) {
    return -1;
  }

  return 0;
}
