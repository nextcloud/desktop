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
#include <assert.h>

#include "csync_private.h"
#include "csync_util.h"
#include "vio/csync_vio.h"
#include "vio/csync_vio_local.h"
#include "csync_statedb.h"
#include "std/c_jhash.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.vio.main"

#include "csync_log.h"
#if USE_NEON
#include "csync_owncloud.h"
#endif

csync_vio_handle_t *csync_vio_opendir(CSYNC *ctx, const char *name) {
  switch(ctx->replica) {
    case REMOTE_REPLICA:
      if(ctx->remote.read_from_db) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Read from db flag is true, should not!" );
      }
      return ctx->callbacks.remote_opendir_hook(name, ctx->callbacks.vio_userdata);
      break;
    case LOCAL_REPLICA:
	if( ctx->callbacks.update_callback ) {
        ctx->callbacks.update_callback(ctx->replica, name, ctx->callbacks.update_callback_userdata);
	}
      return csync_vio_local_opendir(name);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }
  return NULL;
}

int csync_vio_closedir(CSYNC *ctx, csync_vio_handle_t *dhandle) {
  int rc = -1;

  if (dhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  switch(ctx->replica) {
  case REMOTE_REPLICA:
      if( ctx->remote.read_from_db ) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Remote ReadFromDb is true, should not!");
      }
      ctx->callbacks.remote_closedir_hook(dhandle, ctx->callbacks.vio_userdata);
      rc = 0;
      break;
  case LOCAL_REPLICA:
      rc = csync_vio_local_closedir(dhandle);
      break;
  default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }
  return rc;
}

csync_vio_file_stat_t *csync_vio_readdir(CSYNC *ctx, csync_vio_handle_t *dhandle) {
  switch(ctx->replica) {
    case REMOTE_REPLICA:
      if( ctx->remote.read_from_db ) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Remote readfromdb is true, should not!");
      }
      return ctx->callbacks.remote_readdir_hook(dhandle, ctx->callbacks.vio_userdata);
      break;
    case LOCAL_REPLICA:
      return csync_vio_local_readdir(dhandle);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return NULL;
}


int csync_vio_stat(CSYNC *ctx, const char *uri, csync_vio_file_stat_t *buf) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "ERROR: Cannot call remote stat, not implemented");
      assert(ctx->replica != REMOTE_REPLICA);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_stat(uri, buf);
      if (rc < 0) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Local stat failed, errno %d", errno);
      }
      break;
    default:
      break;
  }

  return rc;
}

char *csync_vio_get_status_string(CSYNC *ctx) {
  if(ctx->error_string) {
    return ctx->error_string;
  }
#ifdef USE_NEON
  return owncloud_error_string(ctx);
#endif
  return 0;
}
