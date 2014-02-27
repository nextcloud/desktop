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

#include "csync_private.h"
#include "csync_util.h"
#include "vio/csync_vio.h"
#include "vio/csync_vio_handle_private.h"
#include "vio/csync_vio_local.h"
#include "csync_statedb.h"
#include "std/c_jhash.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#define CSYNC_LOG_CATEGORY_NAME "csync.vio.main"

#ifdef _WIN32
#include <wchar.h>
#define MODULE_EXTENSION "dll"
#else
#define MODULE_EXTENSION "so"
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "csync_log.h"

int csync_vio_init(CSYNC *ctx, const char *module, const char *args) {

  csync_vio_method_t *m = NULL;
  csync_vio_method_init_fn init_fn;

  /* The owncloud module used to be dynamically loaded, but now it's just statically linked */
  extern csync_vio_method_t *vio_module_init(const char *method_name, const char *config_args, csync_auth_callback cb, void *userdata);
  extern void vio_module_shutdown(csync_vio_method_t *);
  init_fn = vio_module_init;
  ctx->module.finish_fn = vio_module_shutdown;

  /* get the method struct */
  m = init_fn(module, args, csync_get_auth_callback(ctx), csync_get_userdata(ctx));
  if (m == NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s returned a NULL method", module);
    return -1;
  }

  /* Some basic checks */
  if (m->method_table_size == 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s method table size is 0", module);
    return -1;
  }

  if (! VIO_METHOD_HAS_FUNC(m, opendir)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s has no opendir fn", module);
    return -1;
  }

  ctx->module.method = m;

  return 0;
}

void csync_vio_shutdown(CSYNC *ctx) {
    /* shutdown the plugin */
    if (ctx->module.finish_fn != NULL) {
      (*ctx->module.finish_fn)(ctx->module.method);
    }

    ctx->module.method = NULL;
    ctx->module.finish_fn = NULL;
}

csync_vio_handle_t *csync_vio_opendir(CSYNC *ctx, const char *name) {
  csync_vio_handle_t *h = NULL;
  csync_vio_method_handle_t *mh = NULL;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      if(ctx->remote.read_from_db) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Read from db flag is true, should not!" );
      }
      mh = ctx->module.method->opendir(name);
      break;
    case LOCAL_REPLICA:
      mh = csync_vio_local_opendir(name);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  h = csync_vio_handle_new(name, mh);
  if (h == NULL) {
    return NULL;
  }

  return h;
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
      rc = ctx->module.method->closedir(dhandle->method_handle);
      break;
  case LOCAL_REPLICA:
      rc = csync_vio_local_closedir(dhandle->method_handle);
      break;
  default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  SAFE_FREE(dhandle->uri);
  SAFE_FREE(dhandle);

  return rc;
}

csync_vio_file_stat_t *csync_vio_readdir(CSYNC *ctx, csync_vio_handle_t *dhandle) {
  csync_vio_file_stat_t *fs = NULL;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      if( ctx->remote.read_from_db ) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Remote readfromdb is true, should not!");
      }
      fs = ctx->module.method->readdir(dhandle->method_handle);
      break;
    case LOCAL_REPLICA:
      fs = csync_vio_local_readdir(dhandle->method_handle);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return fs;
}


int csync_vio_stat(CSYNC *ctx, const char *uri, csync_vio_file_stat_t *buf) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      rc = ctx->module.method->stat(uri, buf);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_stat(uri, buf);
      if (rc < 0) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Local stat failed, errno %d", errno);
      }
#ifdef _WIN32
      else {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Win32: STAT-inode for %s: %llu", uri, buf->inode );
      }
#endif
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
    if(VIO_METHOD_HAS_FUNC(ctx->module.method, get_error_string)) {
        return ctx->module.method->get_error_string();
    }
    return NULL;
}

int csync_vio_set_property(CSYNC* ctx, const char* key, void* data) {
  int rc = -1;
  if(VIO_METHOD_HAS_FUNC(ctx->module.method, set_property))
    rc = ctx->module.method->set_property(key, data);
  return rc;
}

int csync_vio_commit(CSYNC *ctx) {
  int rc = 0;

  if (VIO_METHOD_HAS_FUNC(ctx->module.method, commit)) {
      rc = ctx->module.method->commit();
  }

  return rc;
}
