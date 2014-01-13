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

#ifdef HAVE_CLOCK_GETTIME
# ifdef _POSIX_MONOTONIC_CLOCK
#  define CSYNC_CLOCK CLOCK_MONOTONIC
# else
#  define CSYNC_CLOCK CLOCK_REALTIME
# endif
#endif


int csync_gettime(struct timespec *tp)
{
#ifdef HAVE_CLOCK_GETTIME
	return clock_gettime(CSYNC_CLOCK, tp);
#else
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0) {
		return -1;
	}

	tp->tv_sec = tv.tv_sec;
	tp->tv_nsec = tv.tv_usec * 1000;
#endif
	return 0;
}

#undef CSYNC_CLOCK

/* check time difference between the replicas */
time_t csync_timediff(CSYNC *ctx) {
  time_t timediff = -1;
  char errbuf[256] = {0};
  char *luri = NULL;
  char *ruri = NULL;
  csync_vio_handle_t *fp = NULL;
  csync_vio_file_stat_t *st = NULL;
  csync_vio_handle_t *dp = NULL;

  /* try to open remote dir to get auth */
  ctx->replica = ctx->remote.type;
  dp = csync_vio_opendir(ctx, ctx->remote.uri);
  if (dp == NULL) {
    /*
     * To prevent problems especially with pam_csync we shouldn't try to create the
     * remote directory here. Just fail!
     */
    C_STRERROR(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL,
        "Access dienied to remote uri: %s - %s",
        ctx->remote.uri,
        errbuf);
    ctx->status_code = CSYNC_STATUS_REMOTE_ACCESS_ERROR;
    return -1;
  }
  csync_vio_closedir(ctx, dp);

  if (asprintf(&luri, "%s/.csync_timediff.ctmp", ctx->local.uri) < 0) {
    goto out;
  }

  if (asprintf(&ruri, "%s/.csync_timediff.ctmp", ctx->remote.uri) < 0) {
    goto out;
  }

  /* create temporary file on local */
  ctx->replica = ctx->local.type;
  fp = csync_vio_creat(ctx, luri, 0644);
  if (fp == NULL) {
    C_STRERROR(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL,
        "Unable to create temporary file: %s - %s",
        luri,
        errbuf);
    ctx->status_code = CSYNC_STATUS_LOCAL_CREATE_ERROR;
    goto out;
  }
  csync_vio_close(ctx, fp);

  /* Get the modification time */
  st = csync_vio_file_stat_new();
  if (csync_vio_stat(ctx, luri, st) < 0) {
    C_STRERROR(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL,
        "Synchronisation is not possible! %s - %s",
        luri,
        errbuf);
    ctx->status_code = CSYNC_STATUS_LOCAL_STAT_ERROR;
    goto out;
  }
  timediff = st->mtime;
  csync_vio_file_stat_destroy(st);
  st = NULL;

  /* create temporary file on remote replica */
  ctx->replica = ctx->remote.type;

  fp = csync_vio_creat(ctx, ruri, 0644);
  if (fp == NULL) {
    C_STRERROR(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL,
        "Unable to create temporary file: %s - %s",
        ruri,
        errbuf);
    ctx->status_code = CSYNC_STATUS_REMOTE_CREATE_ERROR;
    goto out;
  }
  csync_vio_close(ctx, fp);

  /* Get the modification time */
  st = csync_vio_file_stat_new();
  if (csync_vio_stat(ctx, ruri, st) < 0) {
    C_STRERROR(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL,
        "Synchronisation is not possible! %s - %s",
        ruri,
        errbuf);
    ctx->status_code = CSYNC_STATUS_REMOTE_STAT_ERROR;
    goto out;
  }

  /* calc time difference */
  timediff = llabs(timediff - st->mtime);
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


/* vim: set ts=8 sw=2 et cindent: */
