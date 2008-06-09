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

static int _csync_cleanup_cmp(const void *a, const void *b) {
  csync_file_stat_t *st_a, *st_b;

  st_a = (csync_file_stat_t *) a;
  st_b = (csync_file_stat_t *) b;

  return strcmp(st_a->path, st_b->path);
}

static int _csync_push_file(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e srep = -1;
  enum csync_replica_e drep = -1;
  enum csync_replica_e rep_bak = -1;

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
  int count = 0;

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

  /* Open the source file */
  ctx->replica = srep;
  sfp = csync_vio_open(ctx, suri, O_RDONLY|O_NOFOLLOW|O_NOATIME, 0);
  if (sfp == NULL) {
    if (errno == ENOMEM) {
      rc = -1;
    } else {
      rc = 1;
    }
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, command: open, error: %s", suri, strerror(errno));
    rc = 1;
    goto out;
  }

  ctx->replica = drep;

  /* create the temporary file name */
  if (asprintf(&turi, "%s.XXXXXX", duri) < 0) {
    rc = -1;
    goto out;
  }
  turi = mktemp(turi);

  /* Create the destination file */
  ctx->replica = drep;
  while ((dfp = csync_vio_open(ctx, turi, O_CREAT|O_EXCL|O_WRONLY|O_NOCTTY, st->mode)) == NULL) {
    switch (errno) {
      case EEXIST:
        if (count++ > 10) {
          rc = 1;
          goto out;
        }
        turi = mktemp(turi);
        break;
      case ENOENT:
        /* get the directory name */
        tdir = c_dirname(turi);
        if (tdir == NULL) {
          rc = -1;
          goto out;
        }

        if (csync_vio_mkdirs(ctx, tdir, 0755) < 0) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "dir: %s, command: mkdirs, error: %s", tdir, strerror(errno));
        }
        break;
      case ENOMEM:
        rc = -1;
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, command: open, error: %s", duri, strerror(errno));
        goto out;
        break;
      default:
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, command: open, error: %s", duri, strerror(errno));
        rc = 1;
        goto out;
        break;
    }

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
    bwritten = csync_vio_write(ctx, dfp, buf, bread);

    if (bwritten < 0 || bread != bwritten) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, command: write, error: bread = %d, bwritten = %d", duri, bread, bwritten);
      rc = 1;
      goto out;
    }
  }

  /*
   * Check filesize
   */
  ctx->replica = drep;
  tstat = csync_vio_file_stat_new();
  if (csync_vio_stat(ctx, turi, tstat) < 0) {
    switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
    }
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, command: stat, error: %s", duri, strerror(errno));
    goto out;
  }

  if (st->size != tstat->size) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, error: incorrect filesize", turi);
    rc = 1;
    goto out;
  }

  /* override original file */
  ctx->replica = drep;
  if (csync_vio_rename(ctx, turi, duri) < 0) {
    switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
    }
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, command: rename, error: %s", duri, strerror(errno));
    goto out;
  }

  /* set owner and group if possible */
  csync_vio_chown(ctx, duri, st->uid, st->gid);

  /* sync time */
  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  ctx->replica = drep;
  csync_vio_utimes(ctx, duri, times);

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

  if (rc != 0) {
    csync_vio_unlink(ctx, turi);
  }

  SAFE_FREE(suri);
  SAFE_FREE(duri);
  SAFE_FREE(turi);
  SAFE_FREE(tdir);

  return rc;
}

static int _csync_new_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;

  rc = _csync_push_file(ctx, st);

  return rc;
}

static int _csync_sync_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;

  rc = _csync_push_file(ctx, st);

  return rc;
}

static int _csync_remove_file(CSYNC *ctx, csync_file_stat_t *st) {
  char *uri = NULL;
  int rc = -1;

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

  if (csync_vio_unlink(ctx, uri) < 0) {
    switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
    }
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file: %s, command: unlink, error: %s", uri, strerror(errno));
    goto out;
  }

  /* FIXME: We can remove the node from the tree */
  st->instruction = CSYNC_INSTRUCTION_NONE;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "file: %s, instruction: REMOVED", uri);

  rc = 0;
out:
  SAFE_FREE(uri);
  return rc;
}

static int _csync_new_dir(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e src;
  enum csync_replica_e dest;
  enum csync_replica_e replica_bak;
  char *uri = NULL;
  struct timeval times[2];
  int rc = -1;

  replica_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      src = ctx->local.type;
      dest = ctx->remote.type;
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLCIA:
      src = ctx->remote.type;
      dest = ctx->local.type;
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  ctx->replica = dest;
  if (csync_vio_mkdirs(ctx, uri, 0755) < 0) {
    switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
    }
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "dir: %s, command: mkdirs, error: %s", uri, strerror(errno));
    goto out;
  }

  /* The chmod is needed here cause the directory could already exist. */
  csync_vio_chmod(ctx, uri, st->mode);

  /* set owner and group if possible */
  csync_vio_chown(ctx, uri, st->uid, st->gid);

  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  csync_vio_utimes(ctx, uri, times);

  st->instruction = CSYNC_INSTRUCTION_NONE;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "dir: %s, instruction: CREATED", uri);
  ctx->replica = replica_bak;

  rc = 0;
out:
  SAFE_FREE(uri);
  return rc;
}

static int _csync_sync_dir(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e src;
  enum csync_replica_e dest;
  enum csync_replica_e replica_bak;
  char *uri = NULL;
  struct timeval times[2];
  int rc = -1;

  replica_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      src = ctx->local.type;
      dest = ctx->remote.type;
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLCIA:
      src = ctx->remote.type;
      dest = ctx->local.type;
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  ctx->replica = dest;

  /* set owner and group if possible */
  csync_vio_chown(ctx, uri, st->uid, st->gid);

  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  csync_vio_utimes(ctx, uri, times);
  if (csync_vio_chmod(ctx, uri, st->mode) < 0) {
    switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
    }
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "dir: %s, command: mkdirs, error: %s", uri, strerror(errno));
    goto out;
  }

  st->instruction = CSYNC_INSTRUCTION_NONE;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "dir: %s, instruction: SYNCED", uri);
  ctx->replica = replica_bak;

  rc = 0;
out:
  SAFE_FREE(uri);
  return rc;
}

static int _csync_remove_dir(CSYNC *ctx, csync_file_stat_t *st) {
  c_list_t *list = NULL;
  char *uri = NULL;
  int rc = -1;

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

  if (csync_vio_rmdir(ctx, uri) < 0) {
    switch (errno) {
      case ENOMEM:
        CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "dir: %s, command: rmdir, error: %s", uri, strerror(errno));
        rc = -1;
        break;
      case ENOTEMPTY:
        switch (ctx->current) {
          case LOCAL_REPLICA:
            list = c_list_prepend(ctx->local.list, (void *) st);
            if (list == NULL) {
              return -1;
            }
            ctx->local.list = list;
            break;
          case REMOTE_REPLCIA:
            list = c_list_prepend(ctx->remote.list, (void *) st);
            if (list == NULL) {
              return -1;
            }
            ctx->remote.list = list;
            break;
          default:
            break;
        }
        rc = 0;
        break;
      default:
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "dir: %s, command: rmdir, error: %s", uri, strerror(errno));
        rc = 1;
        break;
    }
    goto out;
  }

  /* FIXME: we can remove the node form the rbtree */
  st->instruction = CSYNC_INSTRUCTION_NONE;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "dir: %s, instruction: REMOVED", uri);

  rc = 0;
out:
  SAFE_FREE(uri);
  return rc;
}

static int _csync_propagation_cleanup(CSYNC *ctx) {
  c_list_t *list = NULL;
  c_list_t *walk = NULL;
  char *uri = NULL;
  char *dir = NULL;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      list = ctx->local.list;
      uri = ctx->local.uri;
      break;
    case REMOTE_REPLCIA:
      list = ctx->remote.list;
      uri = ctx->remote.uri;
      break;
    default:
      break;
  }

  if (list == NULL) {
    return 0;
  }

  list = c_list_sort(list, _csync_cleanup_cmp);
  if (list == NULL) {
    return -1;
  }

  for (walk = c_list_last(list); walk != NULL; walk = c_list_prev(walk)) {
    csync_file_stat_t *st = NULL;

    st = (csync_file_stat_t *) walk->data;

    if (asprintf(&dir, "%s/%s", uri, st->path) < 0) {
      return -1;
    }

    csync_vio_rmdir(ctx, dir);

    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "dir: %s, instruction: CLEANUP", dir);

    /* FIXME: we can remove the node form the rbtree */
    st->instruction = CSYNC_INSTRUCTION_NONE;

    SAFE_FREE(dir);
  }

  return 0;
}

static int _csync_propagation_file_visitor(void *obj, void *data) {
  csync_file_stat_t *st = NULL;
  CSYNC *ctx = NULL;

  st = (csync_file_stat_t *) obj;
  ctx = (CSYNC *) data;

  switch(st->type) {
    case CSYNC_FTW_TYPE_SLINK:
      break;
    case CSYNC_FTW_TYPE_FILE:
      switch (st->instruction) {
        case CSYNC_INSTRUCTION_NEW:
          if (_csync_new_file(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_SYNC:
          if (_csync_sync_file(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_REMOVE:
          if (_csync_remove_file(ctx, st) < 0) {
            goto err;
          }
          break;
        default:
          break;
      }
      break;
    case CSYNC_FTW_TYPE_DIR:
      /*
       * We have to walk over the files first. If you create or rename a file
       * in a directory on unix. The modification time of the directory gets
       * changed.
       */
      break;
    default:
      break;
  }

  return 0;
err:
  CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "file: %s, error: %s", st->path, strerror(errno));
  return -1;
}

static int _csync_propagation_dir_visitor(void *obj, void *data) {
  csync_file_stat_t *st = NULL;
  CSYNC *ctx = NULL;

  st = (csync_file_stat_t *) obj;
  ctx = (CSYNC *) data;

  switch(st->type) {
    case CSYNC_FTW_TYPE_SLINK:
      /* FIXME: implement symlink support */
      break;
    case CSYNC_FTW_TYPE_FILE:
      break;
    case CSYNC_FTW_TYPE_DIR:
      switch (st->instruction) {
        case CSYNC_INSTRUCTION_NEW:
          if (_csync_new_dir(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_SYNC:
          if (_csync_sync_dir(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_REMOVE:
          if (_csync_remove_dir(ctx, st) < 0) {
            goto err;
          }
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

  if (c_rbtree_walk(tree, (void *) ctx, _csync_propagation_file_visitor) < 0) {
    return -1;
  }

  if (c_rbtree_walk(tree, (void *) ctx, _csync_propagation_dir_visitor) < 0) {
    return -1;
  }

  if (_csync_propagation_cleanup(ctx) < 0) {
    return -1;
  }

  return 0;
}

