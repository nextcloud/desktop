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

#ifndef _CSYNC_VIO_H
#define _CSYNC_VIO_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "c_private.h"
#include "vio/csync_vio_handle.h"
#include "vio/csync_vio_file_stat.h"
#include "csync_private.h"

typedef struct fhandle_s {
  int fd;
} fhandle_t;

int csync_vio_init(CSYNC *ctx, const char *module, const char *args);
void csync_vio_shutdown(CSYNC *ctx);
csync_vio_handle_t *csync_vio_open(CSYNC *ctx, const char *uri, int flags, mode_t mode);
csync_vio_handle_t *csync_vio_creat(CSYNC *ctx, const char *uri, mode_t mode);
int csync_vio_close(CSYNC *ctx, csync_vio_handle_t *handle);
ssize_t csync_vio_read(CSYNC *ctx, csync_vio_handle_t *fhandle, void *buf, size_t count);
ssize_t csync_vio_write(CSYNC *ctx, csync_vio_handle_t *fhandle, const void *buf, size_t count);
int csync_vio_sendfile(CSYNC *ctx,  csync_vio_handle_t *sfp, csync_vio_handle_t *dst);
int64_t csync_vio_lseek(CSYNC *ctx, csync_vio_handle_t *fhandle, int64_t offset, int whence);

int csync_vio_put(CSYNC *ctx, csync_vio_handle_t *flocal, csync_vio_handle_t *fremote, csync_file_stat_t *st);
int csync_vio_get(CSYNC *ctx, csync_vio_handle_t *flocal, csync_vio_handle_t *fremote, csync_file_stat_t *st);

csync_vio_handle_t *csync_vio_opendir(CSYNC *ctx, const char *name);
int csync_vio_closedir(CSYNC *ctx, csync_vio_handle_t *dhandle);
csync_vio_file_stat_t *csync_vio_readdir(CSYNC *ctx, csync_vio_handle_t *dhandle);

int csync_vio_mkdir(CSYNC *ctx, const char *uri, mode_t mode);
int csync_vio_mkdirs(CSYNC *ctx, const char *uri, mode_t mode);
int csync_vio_rmdir(CSYNC *ctx, const char *uri);

const char *csync_vio_get_etag(CSYNC *ctx, const char *path);
int csync_vio_stat(CSYNC *ctx, const char *uri, csync_vio_file_stat_t *buf);
int csync_vio_rename(CSYNC *ctx, const char *olduri, const char *newuri);
int csync_vio_unlink(CSYNC *ctx, const char *uri);

int csync_vio_chmod(CSYNC *ctx, const char *uri, mode_t mode);
int csync_vio_chown(CSYNC *ctx, const char *uri, uid_t owner, gid_t group);

int csync_vio_utimes(CSYNC *ctx, const char *uri, const struct timeval *times);

int csync_vio_set_property(CSYNC *ctx, const char *key, void *data);

char *csync_vio_get_status_string(CSYNC *ctx);

int csync_vio_commit(CSYNC *ctx);

int csync_vio_getfd(csync_vio_handle_t *fhandle);

#endif /* _CSYNC_VIO_H */
