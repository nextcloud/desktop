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
#include "csync_dbtree.h"
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

  /* Useful defaults to the module capabilities */
  ctx->module.capabilities.atomar_copy_support = false;
  ctx->module.capabilities.do_post_copy_stat   = true;
  ctx->module.capabilities.time_sync_required  = true;
  ctx->module.capabilities.unix_extensions     = -1; /* detect automatically */
  ctx->module.capabilities.use_send_file_to_propagate = false; /* do use read/write by default */

  /* Load the module capabilities from the module if it implements the it. */
  if( VIO_METHOD_HAS_FUNC(m, get_capabilities)) {
    ctx->module.capabilities = *(m->get_capabilities());
  }

  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "capabilities: atomar copy support: %s",
            ctx->module.capabilities.atomar_copy_support ? "yes": "no");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "capabilities: post copy stat: %s",
            ctx->module.capabilities.do_post_copy_stat ? "yes": "no");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "capabilities: time sync required: %s",
            ctx->module.capabilities.time_sync_required ? "yes": "no");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "capabilities: unix extensions: %d",
            ctx->module.capabilities.unix_extensions );
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "capabilities: use send_file: %s",
            ctx->module.capabilities.use_send_file_to_propagate ? "yes" : "no" );
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "capabilities: get support: %s",
            ctx->module.capabilities.get_support ? "yes" : "no" );
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "capabilities: put support: %s",
            ctx->module.capabilities.put_support? "yes" : "no" );

  /* Some basic checks */
  if (m->method_table_size == 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s method table size is 0", module);
    return -1;
  }

  if (! VIO_METHOD_HAS_FUNC(m, opendir)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s has no opendir fn", module);
    return -1;
  }

  /* Useful defaults to the module capabilities */
  ctx->module.capabilities.atomar_copy_support = false;
  ctx->module.capabilities.put_support         = false;
  ctx->module.capabilities.get_support         = false;

  /* Load the module capabilities from the module if it implements the it. */
  if( VIO_METHOD_HAS_FUNC(m, get_capabilities)) {
    ctx->module.capabilities = *(m->get_capabilities());
  } else {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "module %s has no capabilities fn", module);
  }

  if (! VIO_METHOD_HAS_FUNC(m, get_etag)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "module %s has no get_etag fn", module);
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

csync_vio_handle_t *csync_vio_open(CSYNC *ctx, const char *uri, int flags, mode_t mode) {
  csync_vio_handle_t *h = NULL;
  csync_vio_method_handle_t *mh = NULL;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      mh = ctx->module.method->open(uri, flags, mode);
      break;
    case LOCAL_REPLICA:
      mh = csync_vio_local_open(uri, flags, mode);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
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
    case REMOTE_REPLICA:
      mh = ctx->module.method->creat(uri, mode);
      break;
    case LOCAL_REPLICA:
      mh = csync_vio_local_creat(uri, mode);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
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
    case REMOTE_REPLICA:
      rc = ctx->module.method->close(fhandle->method_handle);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_close(fhandle->method_handle);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  /* handle->method_handle is free'd by the above close */
  SAFE_FREE(fhandle->uri);
  SAFE_FREE(fhandle);

  return rc;
}

int csync_vio_getfd(csync_vio_handle_t *fhandle) {
  int fd = -1;

  if (fhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  fd = csync_vio_local_getfd( fhandle );
  // Return the correct handle here.
  return fd;

}

int csync_vio_put(CSYNC *ctx,
                  csync_vio_handle_t *flocal,
                  csync_vio_handle_t *fremote,
                  csync_file_stat_t *st) {
  int rc = 0;
  csync_vio_file_stat_t *vfs = csync_vio_convert_file_stat(st);

  if (flocal == NULL) {
    rc = -1;
  }
  if (vfs ==  NULL) {
    rc = -1;
  }

  if (rc == 0) {
    rc = ctx->module.method->put(flocal->method_handle,
                                 fremote->method_handle,
                                 vfs);
  }
  csync_vio_file_stat_destroy(vfs);
  return rc;
}

int csync_vio_get(CSYNC *ctx,
                  csync_vio_handle_t *flocal,
                  csync_vio_handle_t *fremote,
                  csync_file_stat_t *st) {
  int rc = 0;
  csync_vio_file_stat_t *vfs = csync_vio_convert_file_stat(st);

  if (flocal == NULL) {
    rc = -1;
  }
  if (vfs ==  NULL) {
    rc = -1;
  }

  if (rc == 0) {
    rc = ctx->module.method->get(flocal->method_handle,
                                 fremote->method_handle,
                                 vfs);
  }
  csync_vio_file_stat_destroy(vfs);
  return rc;
}

ssize_t csync_vio_read(CSYNC *ctx, csync_vio_handle_t *fhandle, void *buf, size_t count) {
  ssize_t rs = 0;

  if (fhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      rs = ctx->module.method->read(fhandle->method_handle, buf, count);
      break;
    case LOCAL_REPLICA:
      rs = csync_vio_local_read(fhandle->method_handle, buf, count);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
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
    case REMOTE_REPLICA:
      rs = ctx->module.method->write(fhandle->method_handle, buf, count);
      break;
    case LOCAL_REPLICA:
      rs = csync_vio_local_write(fhandle->method_handle, buf, count);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return rs;
}

int csync_vio_sendfile(CSYNC *ctx, csync_vio_handle_t *sfp, csync_vio_handle_t *dst) {
    int rc = 0;

    switch(ctx->replica) {
      case REMOTE_REPLICA:
        rc = ctx->module.method->sendfile(sfp->method_handle, dst->method_handle);
        break;
      case LOCAL_REPLICA:
        rc = ctx->module.method->sendfile(dst->method_handle, sfp->method_handle);
        break;
      default:
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
        break;
    }

    return rc;
}

int64_t csync_vio_lseek(CSYNC *ctx, csync_vio_handle_t *fhandle, int64_t offset, int whence) {
  int64_t ro = 0;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      ro = ctx->module.method->lseek(fhandle->method_handle, offset, whence);
      break;
    case LOCAL_REPLICA:
      ro = csync_vio_local_lseek(fhandle->method_handle, offset, whence);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return ro;
}

csync_vio_handle_t *csync_vio_opendir(CSYNC *ctx, const char *name) {
  csync_vio_handle_t *h = NULL;
  csync_vio_method_handle_t *mh = NULL;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      if(ctx->remote.read_from_db) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Reading directory %s from database", name);
          mh = csync_dbtree_opendir(ctx, name);
      } else {
          mh = ctx->module.method->opendir(name);
      }
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
      if(ctx->remote.read_from_db) {
          rc = csync_dbtree_closedir(ctx, dhandle->method_handle);
      } else {
          rc = ctx->module.method->closedir(dhandle->method_handle);
      }
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
      if(ctx->remote.read_from_db) {
          fs = csync_dbtree_readdir(ctx, dhandle->method_handle);
      } else {
          fs = ctx->module.method->readdir(dhandle->method_handle);
      }
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

int csync_vio_mkdir(CSYNC *ctx, const char *uri, mode_t mode) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      rc = ctx->module.method->mkdir(uri, mode);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_mkdir(uri, mode);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return rc;
}

int csync_vio_mkdirs(CSYNC *ctx, const char *uri, mode_t mode) {
  int tmp = 0;
  char errbuf[256] = {0};
  csync_vio_file_stat_t *st = NULL;

  if (uri == NULL) {
    errno = EINVAL;
    return -1;
  }

  tmp = strlen(uri);
  while(tmp > 0 && uri[tmp - 1] == '/') --tmp;
  while(tmp > 0 && uri[tmp - 1] != '/') --tmp;
  while(tmp > 0 && uri[tmp - 1] == '/') --tmp;

  if (tmp > 0) {
    char suburi[tmp + 1];
    memcpy(suburi, uri, tmp);
    suburi[tmp] = '\0';

    st = csync_vio_file_stat_new();
    if (csync_vio_stat(ctx, suburi, st) == 0) {
      if (! S_ISDIR(st->mode)) {
        csync_vio_file_stat_destroy(st);
        errno = ENOTDIR;
        return -1;
      }
    } else if (errno != ENOENT) {
      C_STRERROR(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "csync_vio_mkdirs stat failed: %s",
          errbuf);
      csync_vio_file_stat_destroy(st);
      return -1;
    } else if (csync_vio_mkdirs(ctx, suburi, mode) < 0) {
      csync_vio_file_stat_destroy(st);
      return -1;
    }
    csync_vio_file_stat_destroy(st);
  }

  tmp = csync_vio_mkdir(ctx, uri, mode);
  if ((tmp < 0) && (errno == EEXIST)) {
    return 0;
  }

  return tmp;
}

int csync_vio_rmdir(CSYNC *ctx, const char *uri) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      rc = ctx->module.method->rmdir(uri);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_rmdir(uri);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return rc;
}

const char *csync_vio_get_etag(CSYNC *ctx, const char *path)
{
    const char *re = NULL;
    /* We always use the remote method here. */
    if(ctx->module.method &&
            VIO_METHOD_HAS_FUNC(ctx->module.method, get_etag)) {
        re = ctx->module.method->get_etag(path);
    }
    return re;
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

int csync_vio_rename(CSYNC *ctx, const char *olduri, const char *newuri) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      rc = ctx->module.method->rename(olduri, newuri);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_rename(olduri, newuri);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return rc;
}

int csync_vio_unlink(CSYNC *ctx, const char *uri) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      rc = ctx->module.method->unlink(uri);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_unlink(uri);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return rc;
}

int csync_vio_chmod(CSYNC *ctx, const char *uri, mode_t mode) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      rc = ctx->module.method->chmod(uri, mode);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_chmod(uri, mode);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return rc;
}

int csync_vio_chown(CSYNC *ctx, const char *uri, uid_t owner, gid_t group) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      rc = ctx->module.method->chown(uri, owner, group);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_chown(uri, owner, group);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
      break;
  }

  return rc;
}

int csync_vio_utimes(CSYNC *ctx, const char *uri, const struct timeval *times) {
  int rc = -1;

  switch(ctx->replica) {
    case REMOTE_REPLICA:
      rc = ctx->module.method->utimes(uri, times);
      break;
    case LOCAL_REPLICA:
      rc = csync_vio_local_utimes(uri, times);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "Invalid replica (%d)", (int)ctx->replica);
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
