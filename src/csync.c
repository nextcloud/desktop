/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2006-2008 by Andreas Schneider <mail@cynapses.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * vim: ts=2 sw=2 et cindent
 */

#define _GNU_SOURCE /* asprintf */

#include <errno.h>
#include <fnmatch.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "c_lib.h"
#include "csync_private.h"
#include "csync_config.h"
#include "csync_exclude.h"
#include "csync_lock.h"
#include "csync_journal.h"
#include "csync_util.h"

#include "csync_update.h"

#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.api"
#include "csync_log.h"

static int key_cmp(const void *key, const void *data) {
  uint64_t a;
  csync_file_stat_t *b;

  a = POINTER_TO_INT(key);
  b = (csync_file_stat_t *) data;

  if (a < b->phash) {
    return -1;
  } else if (a > b->phash) {
    return 1;
  }

  return 0;
}

static int data_cmp(const void *key, const void *data) {
  csync_file_stat_t *a, *b;

  a = (csync_file_stat_t *) key;
  b = (csync_file_stat_t *) data;

  if (a->phash < b->phash) {
    return -1;
  } else if (a->phash > b->phash) {
    return 1;
  }

  return 0;
}

int csync_create(CSYNC **csync, const char *local, const char *remote) {
  CSYNC *ctx;
  size_t len = 0;

  ctx = c_malloc(sizeof(CSYNC));
  if (ctx == NULL) {
    return -1;
  }

  /* remove trailing slashes */
  len = strlen(local);
  while(len > 0 && local[len - 1] == '/') --len;

  ctx->local.uri = c_strndup(local, len);
  if (ctx->local.uri == NULL) {
    return -1;
  }

  /* remove trailing slashes */
  len = strlen(remote);
  while(len > 0 && remote[len - 1] == '/') --len;

  ctx->remote.uri = c_strndup(remote, len);
  if (ctx->remote.uri == NULL) {
    SAFE_FREE(ctx->remote.uri);
    return -1;
  }

  ctx->options.max_depth = MAX_DEPTH;
  ctx->options.max_time_difference = MAX_TIME_DIFFERENCE;

  if (asprintf(&ctx->options.config_dir, "%s/%s", getenv("HOME"), CSYNC_CONF_DIR) < 0) {
    SAFE_FREE(ctx->local.uri);
    SAFE_FREE(ctx->remote.uri);
    SAFE_FREE(ctx);
    errno = ENOMEM;
    return -1;
  }

  *csync = ctx;
  return 0;
}

int csync_init(CSYNC *ctx) {
  int rc;
  char *log = NULL;
  char *exclude = NULL;
  char *journal = NULL;
  char *lock = NULL;
  char *config = NULL;

  if (ctx == NULL) {
    errno = EBADF;
    return -1;
  }

  /* Do not initialize twice */
  if (ctx->status & CSYNC_INIT) {
    return 1;
  }

  /* load log file */
  if (csync_log_init() < 0) {
    fprintf(stderr, "csync_init: logger init failed\n");
    return -1;
  }

  /* create dir if it doesn't exist */
  if (! c_isdir(ctx->options.config_dir)) {
    c_mkdirs(ctx->options.config_dir, 0700);
  }

  if (asprintf(&log, "%s/%s", ctx->options.config_dir, CSYNC_LOG_FILE) < 0) {
    rc = -1;
    goto out;
  }

  /* load log if it exists */
  if (c_isfile(log)) {
    csync_log_load(log);
  } else {
    if (c_copy(SYSCONFDIR "/csync/" CSYNC_LOG_FILE, log, 0644) == 0) {
      csync_log_load(log);
    }
  }

  /* create lock file */
  if (asprintf(&lock, "%s/%s", ctx->options.config_dir, CSYNC_LOCK_FILE) < 0) {
    rc = -1;
    goto out;
  }

  if (csync_lock(lock) < 0) {
    rc = -1;
    goto out;
  }

  /* load config file */
  if (asprintf(&config, "%s/%s", ctx->options.config_dir, CSYNC_CONF_FILE) < 0) {
    rc = -1;
    goto out;
  }

  if (csync_config_load(ctx, config) < 0) {
    rc = -1;
    goto out;
  }

  /* load global exclude list */
  if (asprintf(&exclude, "%s/csync/%s", SYSCONFDIR, CSYNC_EXCLUDE_FILE) < 0) {
    rc = -1;
    goto out;
  }

  if (csync_exclude_load(ctx, exclude) < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Could not load %s - %s", exclude, strerror(errno));
  }
  SAFE_FREE(exclude);

  /* load exclude list */
  if (asprintf(&exclude, "%s/%s", ctx->options.config_dir, CSYNC_CONF_FILE) < 0) {
    rc = -1;
    goto out;
  }

  if (csync_exclude_load(ctx, exclude) < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Could not load %s - %s", exclude, strerror(errno));
  }

  /* create/load journal */
  if (asprintf(&journal, "%s/%s", ctx->options.config_dir, CSYNC_JOURNAL_FILE) < 0) {
    rc = -1;
    goto out;
  }

  if (csync_journal_load(ctx, journal) < 0) {
    rc = -1;
    goto out;
  }

  ctx->local.type = LOCAL_REPLICA;

  /* check for uri */
  if (fnmatch("*://*", ctx->remote.uri, 0) == 0) {
    size_t len;
    len = strstr(ctx->remote.uri, "://") - ctx->remote.uri;
    /* get protocol */
    if (len > 0) {
      char *module = NULL;
      /* module name */
      module = c_strndup(ctx->remote.uri, len);
      if (module == NULL) {
        rc = -1;
        goto out;
      }
      /* load module */
      rc = csync_vio_init(ctx, module, NULL);
      SAFE_FREE(module);
      if (rc < 0) {
        goto out;
      }
      ctx->remote.type = REMOTE_REPLCIA;
    }
  } else {
    ctx->remote.type = LOCAL_REPLICA;
  }

  if (c_rbtree_create(&ctx->local.tree, key_cmp, data_cmp) < 0) {
    rc = -1;
    goto out;
  }

  if (c_rbtree_create(&ctx->remote.tree, key_cmp, data_cmp) < 0) {
    rc = -1;
    goto out;
  }

  ctx->status = CSYNC_INIT;

  rc = 0;

out:
  SAFE_FREE(log);
  SAFE_FREE(lock);
  SAFE_FREE(exclude);
  SAFE_FREE(journal);
  SAFE_FREE(config);
  return rc;
}

int csync_update(CSYNC *ctx) {
  time_t start, finish;

  if (ctx == NULL) {
    errno = EBADF;
    return -1;
  }

  csync_memstat_check();

  time(&start);
  ctx->current = LOCAL_REPLICA;
  ctx->replica = ctx->local.type;
  if (csync_ftw(ctx, ctx->local.uri, csync_walker, MAX_DEPTH) < 0) {
    return -1;
  }
  time(&finish);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
            "Update detection for local replica took %.2f seconds",
            difftime(finish, start));
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
            "Collected files: %lu", c_rbtree_size(ctx->local.tree));
  csync_memstat_check();

  time(&start);
  ctx->current = REMOTE_REPLCIA;
  ctx->replica = ctx->remote.type;
  if (csync_ftw(ctx, ctx->remote.uri, csync_walker, MAX_DEPTH) < 0) {
    return -1;
  }
  time(&finish);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
            "Update detection for remote replica took %.2f seconds",
            difftime(finish, start));
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
            "Collected files: %lu", c_rbtree_size(ctx->remote.tree));
  csync_memstat_check();

  ctx->status |= CSYNC_UPDATE;

  return 0;
}

static void tree_destructor(void *data) {
  csync_file_stat_t *freedata = NULL;

  freedata = (csync_file_stat_t *) data;
  SAFE_FREE(freedata);
}

int csync_destroy(CSYNC *ctx) {
  char *lock = NULL;

  if (ctx == NULL) {
    errno = EBADF;
    return -1;
  }

  csync_vio_shutdown(ctx);

  /* TODO: write journal */

  if (ctx->journal.db != NULL) {
    sqlite3_close(ctx->journal.db);
    /* TODO if we successfully synchronized, overwrite the original journal */
  }

  csync_exclude_destroy(ctx);

  if (asprintf(&lock, "%s/%s", ctx->options.config_dir, CSYNC_LOCK_FILE) > 0) {
    csync_lock_remove(lock);
  }

  csync_log_fini();

  /* destroy the rbtrees */
  if (c_rbtree_size(ctx->local.tree) > 0) {
    c_rbtree_destroy(ctx->local.tree, tree_destructor);
  }

  if (c_rbtree_size(ctx->remote.tree) > 0) {
    c_rbtree_destroy(ctx->remote.tree, tree_destructor);
  }

  c_rbtree_free(ctx->local.tree);
  c_rbtree_free(ctx->remote.tree);
  SAFE_FREE(ctx->local.uri);
  SAFE_FREE(ctx->remote.uri);
  SAFE_FREE(ctx->options.config_dir);

  SAFE_FREE(ctx);

  SAFE_FREE(lock);

  return 0;
}

const char *csync_version(void) {
  return CSYNC_VERSION_STRING;
}

