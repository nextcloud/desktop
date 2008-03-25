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
#include <stdio.h>
#include <string.h>

#include "config.h"

#include "c_lib.h"
#include "csync_private.h"
#include "csync_config.h"
#include "csync_lock.h"
#include "csync_exclude.h"
#include "csync_journal.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.api"
#include "csync_log.h"

int csync_create(CSYNC **csync) {
  CSYNC *ctx;

  ctx = c_malloc(sizeof(CSYNC));
  if (ctx == NULL) {
    errno = ENOMEM;
    return -1;
  }

  ctx->options.max_depth = MAX_DEPTH;
  ctx->options.max_time_difference = MAX_TIME_DIFFERENCE;

  if (asprintf(&ctx->options.config_dir, "%s/%s", getenv("HOME"), CSYNC_CONF_DIR) < 0) {
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
  if (ctx->initialized) {
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

  /* TODO: load plugins */

  ctx->initialized = 1;

  rc = 0;

out:
  SAFE_FREE(log);
  SAFE_FREE(lock);
  SAFE_FREE(exclude);
  SAFE_FREE(journal);
  SAFE_FREE(config);
  return rc;
}

int csync_destroy(CSYNC *ctx) {
  char *lock = NULL;

  /* TODO: write journal */

  if (ctx->journal) {
    sqlite3_close(ctx->journal);
    /* TODO if we successfully synchronized, overwrite the original journal */
  }

  if (asprintf(&lock, "%s/%s", ctx->options.config_dir, CSYNC_LOCK_FILE) > 0) {
    csync_lock_remove(lock);
  }

  csync_log_fini();

  SAFE_FREE(ctx->options.config_dir);

  SAFE_FREE(ctx);

  SAFE_FREE(lock);

  return 0;
}

const char *csync_version(void) {
  return CSYNC_VERSION_STRING;
}

