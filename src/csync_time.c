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
#include <time.h>

#include "csync_time.h"
#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.time"
#include "csync_log.h"

/* check time difference between the replicas */
time_t csync_timediff(CSYNC *ctx) {
  time_t timediff = -1;
  char *luri = NULL;
  char *ruri = NULL;
  csync_vio_handle_t *fp = NULL;
  csync_vio_file_stat_t *st = NULL;
  csync_vio_handle_t *dp = NULL;

  /* try to open remote dir to get auth */
  ctx->replica = ctx->remote.type;
  dp = csync_vio_opendir(ctx, ctx->remote.uri);
  if (dp == NULL) {
    if (csync_vio_mkdir(ctx, ctx->remote.uri, 0755) < 0) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "Unable to create directory on remote uri: %s - %s", luri, strerror(errno));
      return -1;
    }
  }
  csync_vio_closedir(ctx, dp);

  if (asprintf(&luri, "%s/csync_timediff.ctmp", ctx->local.uri) < 0) {
    goto out;
  }

  if (asprintf(&ruri, "%s/csync_timediff.ctmp", ctx->remote.uri) < 0) {
    goto out;
  }

  /* create temporary file on local */
  ctx->replica = ctx->local.type;
  fp = csync_vio_creat(ctx, luri, 0644);
  if (fp == NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "Unable to create temporary file: %s - %s", luri, strerror(errno));
    goto out;
  }
  csync_vio_close(ctx, fp);

  /* Get the modification time */
  st = csync_vio_file_stat_new();
  if (csync_vio_stat(ctx, luri, st) < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "Synchronisation is not possible! %s - %s", luri, strerror(errno));
    goto out;
  }
  timediff = st->mtime;
  csync_vio_file_stat_destroy(st);
  st = NULL;

  /* create temporary file on remote replica */
  ctx->replica = ctx->remote.type;

  fp = csync_vio_creat(ctx, ruri, 0644);
  if (fp == NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "Unable to create temporary file: %s - %s", ruri, strerror(errno));
    goto out;
  }
  csync_vio_close(ctx, fp);

  /* Get the modification time */
  st = csync_vio_file_stat_new();
  if (csync_vio_stat(ctx, ruri, st) < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "Synchronisation is not possible! %s - %s", ruri, strerror(errno));
    goto out;
  }

  /* calc time difference */
  timediff = abs(timediff - st->mtime);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Time difference: %ld seconds", timediff);

out:
  csync_vio_file_stat_destroy(st);

  ctx->replica = ctx->local.type;
  csync_vio_unlink(ctx, luri);
  SAFE_FREE(luri);

  ctx->replica = ctx->remote.type;
  csync_vio_unlink(ctx, ruri);
  SAFE_FREE(ruri);

  return timediff;
}

struct timespec csync_tsub(struct timespec timespec1, struct timespec timespec2) {
  struct timespec ret;
  int xsec = 0;
  int sign = 1;

  if (timespec2.tv_nsec > timespec1.tv_nsec) {
    xsec = (int) ((timespec2.tv_nsec - timespec1.tv_nsec) / (1E9 + 1));
    timespec2.tv_nsec -= (long int) (1E9 * xsec);
    timespec2.tv_sec += xsec;
  }

  if ((timespec1.tv_nsec - timespec2.tv_nsec) > 1E9) {
    xsec = (int) ((timespec1.tv_nsec - timespec2.tv_nsec) / 1E9);
    timespec2.tv_nsec += (long int) (1E9 * xsec);
    timespec2.tv_sec -= xsec;
  }

  ret.tv_sec = timespec1.tv_sec - timespec2.tv_sec;
  ret.tv_nsec = timespec1.tv_nsec - timespec2.tv_nsec;

  if (timespec1.tv_sec < timespec2.tv_sec) {
    sign = -1;
  }

  ret.tv_sec = ret.tv_sec * sign;

  return ret;
}

double csync_secdiff(struct timespec clock1, struct timespec clock2) {
  double ret;
  struct timespec diff;

  diff = csync_tsub(clock1, clock2);

  ret = diff.tv_sec;
  ret += (double) diff.tv_nsec / (double) 1E9;

  return ret;
}


