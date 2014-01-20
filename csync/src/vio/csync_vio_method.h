/*
 * cynapses libc functions
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

#ifndef _CSYNC_VIO_METHOD_H
#define _CSYNC_VIO_METHOD_H

#include <sys/time.h>

#include "csync.h"
#include "vio/csync_vio_file_stat.h"
#include "vio/csync_vio_handle.h"

#define VIO_METHOD_HAS_FUNC(method,func) \
  (method != NULL && method->func != NULL \
   && ((size_t)(((char *)&((method)->func)) - ((char *)(method))) < (method)->method_table_size))

typedef struct csync_vio_method_s csync_vio_method_t;

/* module capabilities definition.
 * remember to set useful defaults in csync_vio.c if you add something here. */
struct csync_vio_capabilities_s {
 bool atomar_copy_support; /* set to true if the backend provides atomar copy */
 bool do_post_copy_stat;   /* true if csync should check the copy afterwards  */
 bool time_sync_required;  /* true if local and remote need to be time synced */
 int  unix_extensions;     /* -1: do csync detection, 0: no unix extensions,
                               1: extensions available */
 bool use_send_file_to_propagate; /* if set, the module rather copies files using send_file than read and write */
 bool get_support;
 bool put_support;
};

typedef struct csync_vio_capabilities_s csync_vio_capabilities_t;

typedef csync_vio_method_t *(*csync_vio_method_init_fn)(const char *method_name,
    const char *config_args, csync_auth_callback cb, void *userdata);
typedef void (*csync_vio_method_finish_fn)(csync_vio_method_t *method);

typedef csync_vio_capabilities_t *(*csync_method_get_capabilities_fn)(void);
typedef const char* (*csync_method_get_etag_fn)(const char* path);
typedef csync_vio_method_handle_t *(*csync_method_open_fn)(const char *durl, int flags, mode_t mode);
typedef csync_vio_method_handle_t *(*csync_method_creat_fn)(const char *durl, mode_t mode);
typedef int (*csync_method_close_fn)(csync_vio_method_handle_t *fhandle);
typedef ssize_t (*csync_method_read_fn)(csync_vio_method_handle_t *fhandle, void *buf, size_t count);
typedef ssize_t (*csync_method_write_fn)(csync_vio_method_handle_t *fhandle, const void *buf, size_t count);
typedef int (*csync_method_sendfile_fn)(csync_vio_method_handle_t *src, csync_vio_method_handle_t *dst);
typedef int64_t (*csync_method_lseek_fn)(csync_vio_method_handle_t *fhandle, int64_t offset, int whence);

typedef csync_vio_method_handle_t *(*csync_method_opendir_fn)(const char *name);
typedef int (*csync_method_closedir_fn)(csync_vio_method_handle_t *dhandle);
typedef csync_vio_file_stat_t *(*csync_method_readdir_fn)(csync_vio_method_handle_t *dhandle);

typedef int (*csync_method_mkdir_fn)(const char *uri, mode_t mode);
typedef int (*csync_method_rmdir_fn)(const char *uri);

typedef int (*csync_method_stat_fn)(const char *uri, csync_vio_file_stat_t *buf);
typedef int (*csync_method_rename_fn)(const char *olduri, const char *newuri);
typedef int (*csync_method_unlink_fn)(const char *uri);

typedef int (*csync_method_chmod_fn)(const char *uri, mode_t mode);
typedef int (*csync_method_chown_fn)(const char *uri, uid_t owner, gid_t group);

typedef int (*csync_method_utimes_fn)(const char *uri, const struct timeval times[2]);

typedef int (*csync_method_set_property_fn)(const char *key, void *data);

typedef char* (*csync_method_get_error_string_fn)();

typedef int (*csync_method_commit_fn)();

typedef int (*csync_method_get_fn)(csync_vio_method_handle_t *flocal,
                                   csync_vio_method_handle_t *fremote,
                                   csync_vio_file_stat_t *st);
typedef int (*csync_method_put_fn)(csync_vio_method_handle_t *flocal,
                                   csync_vio_method_handle_t *fremote,
                                   csync_vio_file_stat_t *st);

struct csync_vio_method_s {
  size_t method_table_size;           /* Used for versioning */
  csync_method_get_capabilities_fn get_capabilities;
  csync_method_open_fn open;
  csync_method_creat_fn creat;
  csync_method_close_fn close;
  csync_method_read_fn read;
  csync_method_write_fn write;
  csync_method_lseek_fn lseek;
  csync_method_opendir_fn opendir;
  csync_method_closedir_fn closedir;
  csync_method_readdir_fn readdir;
  csync_method_mkdir_fn mkdir;
  csync_method_rmdir_fn rmdir;
  csync_method_stat_fn stat;
  csync_method_rename_fn rename;
  csync_method_unlink_fn unlink;
  csync_method_chmod_fn chmod;
  csync_method_chown_fn chown;
  csync_method_utimes_fn utimes;
  csync_method_set_property_fn set_property;
  csync_method_get_error_string_fn get_error_string;
  csync_method_commit_fn commit;
  csync_method_put_fn put;
  csync_method_get_fn get;
  csync_method_get_etag_fn get_etag;
  csync_method_sendfile_fn sendfile;
};

#endif /* _CSYNC_VIO_H */
