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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <QLoggingCategory>
#include "common/asserts.h"

#include "csync_private.h"
#include "csync_util.h"
#include "vio/csync_vio.h"
#include "vio/csync_vio_local.h"
#include "csync_statedb.h"
#include "common/c_jhash.h"

Q_LOGGING_CATEGORY(lcVio, "sync.csync.vio", QtInfoMsg)

csync_vio_handle_t *csync_vio_opendir(CSYNC *ctx, const char *name) {
  switch(ctx->current) {
    case REMOTE_REPLICA:
      ASSERT(!ctx->remote.read_from_db);
      return ctx->callbacks.remote_opendir_hook(name, ctx->callbacks.vio_userdata);
      break;
    case LOCAL_REPLICA:
	if( ctx->callbacks.update_callback ) {
        ctx->callbacks.update_callback(ctx->current, name, ctx->callbacks.update_callback_userdata);
	}
      return csync_vio_local_opendir(name);
      break;
    default:
      ASSERT(false);
  }
  return NULL;
}

int csync_vio_closedir(CSYNC *ctx, csync_vio_handle_t *dhandle) {
  int rc = -1;

  if (dhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  switch(ctx->current) {
  case REMOTE_REPLICA:
      ASSERT(!ctx->remote.read_from_db);
      ctx->callbacks.remote_closedir_hook(dhandle, ctx->callbacks.vio_userdata);
      rc = 0;
      break;
  case LOCAL_REPLICA:
      rc = csync_vio_local_closedir(dhandle);
      break;
  default:
      ASSERT(false);
      break;
  }
  return rc;
}

std::unique_ptr<csync_file_stat_t> csync_vio_readdir(CSYNC *ctx, csync_vio_handle_t *dhandle) {
  switch(ctx->current) {
    case REMOTE_REPLICA:
      ASSERT(!ctx->remote.read_from_db);
      return ctx->callbacks.remote_readdir_hook(dhandle, ctx->callbacks.vio_userdata);
      break;
    case LOCAL_REPLICA:
      return csync_vio_local_readdir(dhandle);
      break;
    default:
      ASSERT(false);
  }

  return NULL;
}

int csync_vio_stat(CSYNC *ctx, const char *uri, csync_file_stat_t *buf) {
  int rc = -1;

    ASSERT(ctx->current == LOCAL_REPLICA);
    rc = csync_vio_local_stat(uri, buf);
    if (rc < 0)
        qCWarning(lcVio, "Local stat failed, errno %d for %s", errno, uri);

  return rc;
}

char *csync_vio_get_status_string(CSYNC *ctx) {
  if(ctx->error_string) {
    return ctx->error_string;
  }
  return 0;
}
