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
#include <dlfcn.h> /* dlopen(), dlclose(), dlsym() ... */

#include "csync_private.h"
#include "vio/csync_vio.h"
#include "vio/csync_vio_handle_private.h"
#include "vio/csync_vio_local.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.vio.main"
#include "csync_log.h"

int csync_vio_init(CSYNC *ctx, const char *module, const char *args) {
  struct stat sb;
  char *path = NULL;
  char *err = NULL;
  csync_vio_method_t *m = NULL;
  csync_vio_method_init_fn init_fn;

  if (asprintf(&path, "%s/csync_%s.so", PLUGINDIR, module) < 0) {
    return -1;
  }

  if (lstat(path, &sb) < 0) {
    SAFE_FREE(path);
    if (asprintf(&path, "%s/modules/csync_%s.so", BINARYDIR, module) < 0) {
      return -1;
    }
  }

  ctx->module.handle = dlopen(path, RTLD_LAZY);
  SAFE_FREE(path);
  if ((err = dlerror()) != NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "loading %s plugin failed - %s",
             module, err);
    return -1;
  }


  init_fn = dlsym(ctx->module.handle, "vio_module_init");
  if ((err = dlerror()) != NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "loading function failed - %s", err);
    return -1;
  }

  ctx->module.finish_fn = dlsym(ctx->module.handle, "vio_module_shutdown");
  if ((err = dlerror()) != NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "loading function failed - %s", err);
    return -1;
  }

  /* get the method struct */
  m = (*init_fn)(module, args, ctx->auth_callback);
  if (m == NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s returned a NULL method", module);
    return -1;
  }

  /* Some basic checks */
  if (m->method_table_size == 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s method table size is 0", module);
    return -1;
  }

  if (! VIO_METHOD_HAS_FUNC(m, open)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s has no open fn", module);
    return -1;
  }

  if (! VIO_METHOD_HAS_FUNC(m, opendir)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s has no opendir fn", module);
    return -1;
  }

  if (! VIO_METHOD_HAS_FUNC(m, open)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s has no stat fn", module);
    return -1;
  }

  ctx->module.method = m;

  return 0;
}

void csync_vio_shutdown(CSYNC *ctx) {
  if (ctx->module.handle != NULL) {
    /* shutdown the plugin */
    if (ctx->module.finish_fn != NULL) {
      (*ctx->module.finish_fn)(ctx->module.method);
    }

    /* close the plugin */
    dlclose(ctx->module.handle);
    ctx->module.handle = NULL;

    ctx->module.method = NULL;
    ctx->module.finish_fn = NULL;
  }
}

csync_vio_handle_t *csync_vio_open(CSYNC *ctx, const char *uri, int flags, mode_t mode) {
  csync_vio_handle_t *h = NULL;
  csync_vio_method_handle_t *mh = NULL;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      mh = ctx->module.method->open(uri, flags, mode);
      break;
    case LOCAL_REPLICA:
      mh = csync_vio_local_open(uri, flags, mode);
      break;
    default:
      break;
  }

  h = csync_vio_handle_new(uri, mh);
  if (h == NULL) {
    return NULL;
  }

  return h;
}

csync_vio_handle_t *csync_vio_creat(CSYNC *ctx, const char *uri, mode_t mode) {
  csync_vio_handle_t *h = NULL;
  csync_vio_method_handle_t *mh = NULL;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      mh = ctx->module.method->creat(uri, mode);
      break;
    case LOCAL_REPLICA:
      mh = csync_vio_local_creat(uri, mode);
      break;
    default:
      break;
  }

  h = csync_vio_handle_new(uri, mh);
  if (h == NULL) {
    return NULL;
  }

  return h;
}

int csync_vio_close(CSYNC *ctx, csync_vio_handle_t *fhandle) {
  int rc = -1;

  if (fhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rc = ctx->module.method->close(fhandle->method_handle);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_close(fhandle->method_handle);
      break;
    default:
      break;
  }

  /* handle->method_handle is free'd by the above close */
  SAFE_FREE(fhandle->uri);
  SAFE_FREE(fhandle);

  return rc;
}

ssize_t csync_vio_read(CSYNC *ctx, csync_vio_handle_t *fhandle, void *buf, size_t count) {
  ssize_t rs = 0;

  if (fhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rs = ctx->module.method->read(fhandle->method_handle, buf, count);
      break;
    case LOCAL_REPLICA:
      rs = csync_vio_local_read(fhandle->method_handle, buf, count);
      break;
    default:
      break;
  }

  return rs;
}

ssize_t csync_vio_write(CSYNC *ctx, csync_vio_handle_t *fhandle, const void *buf, size_t count) {
  ssize_t rs = 0;

  if (fhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rs = ctx->module.method->write(fhandle->method_handle, buf, count);
      break;
    case LOCAL_REPLICA:
      rs = csync_vio_local_write(fhandle->method_handle, buf, count);
      break;
    default:
      break;
  }

  return rs;
}

off_t csync_vio_lseek(CSYNC *ctx, csync_vio_handle_t *fhandle, off_t offset, int whence) {
  off_t ro = 0;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      ro = ctx->module.method->lseek(fhandle->method_handle, offset, whence);
      break;
    case LOCAL_REPLICA:
      ro = csync_vio_local_lseek(fhandle->method_handle, offset, whence);
      break;
    default:
      break;
  }

  return ro;
}

csync_vio_handle_t *csync_vio_opendir(CSYNC *ctx, const char *name) {
  csync_vio_handle_t *h = NULL;
  csync_vio_method_handle_t *mh = NULL;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      mh = ctx->module.method->opendir(name);
      break;
    case LOCAL_REPLICA:
      mh = csync_vio_local_opendir(name);
      break;
    default:
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
    case REMOTE_REPLCIA:
      rc = ctx->module.method->closedir(dhandle->method_handle);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_closedir(dhandle->method_handle);
      break;
    default:
      break;
  }

  SAFE_FREE(dhandle->uri);
  SAFE_FREE(dhandle);

  return rc;
}

csync_vio_file_stat_t *csync_vio_readdir(CSYNC *ctx, csync_vio_handle_t *dhandle) {
  csync_vio_file_stat_t *fs = NULL;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      fs = ctx->module.method->readdir(dhandle->method_handle);
      break;
    case LOCAL_REPLICA:
      fs = csync_vio_local_readdir(dhandle->method_handle);
      break;
    default:
      break;
  }

  return fs;
}

int csync_vio_mkdir(CSYNC *ctx, const char *uri, mode_t mode) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rc = ctx->module.method->mkdir(uri, mode);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_mkdir(uri, mode);
      break;
    default:
      break;
  }

  return rc;
}

int csync_vio_rmdir(CSYNC *ctx, const char *uri) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rc = ctx->module.method->rmdir(uri);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_rmdir(uri);
      break;
    default:
      break;
  }

  return rc;
}

int csync_vio_stat(CSYNC *ctx, const char *uri, csync_vio_file_stat_t *buf) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rc = ctx->module.method->stat(uri, buf);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_stat(uri, buf);
      break;
    default:
      break;
  }

  return rc;
}

int csync_vio_rename(CSYNC *ctx, const char *olduri, const char *newuri) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rc = ctx->module.method->rename(olduri, newuri);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_rename(olduri, newuri);
      break;
    default:
      break;
  }

  return rc;
}

int csync_vio_unlink(CSYNC *ctx, const char *uri) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rc = ctx->module.method->unlink(uri);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_unlink(uri);
      break;
    default:
      break;
  }

  return rc;
}

int csync_vio_chmod(CSYNC *ctx, const char *uri, mode_t mode) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rc = ctx->module.method->chmod(uri, mode);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_chmod(uri, mode);
      break;
    default:
      break;
  }

  return rc;
}

int csync_vio_chown(CSYNC *ctx, const char *uri, uid_t owner, gid_t group) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rc = ctx->module.method->chown(uri, owner, group);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_chown(uri, owner, group);
      break;
    default:
      break;
  }

  return rc;
}

int csync_vio_utimes(CSYNC *ctx, const char *uri, const struct timeval *times) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLCIA:
      rc = ctx->module.method->utimes(uri, times);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_utimes(uri, times);
      break;
    default:
      break;
  }

  return rc;
}

