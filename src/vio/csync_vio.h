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
 * vim: ft=c.doxygen ts=2 sw=2 et cindent
 */

#ifndef _CSYNC_VIO_H
#define _CSYNC_VIO_H

#include "vio/csync_vio_handle.h"
#include "vio/csync_vio_file_stat.h"

int csync_vio_init(void);
int csync_vio_shutdown(void);

csync_vio_handle_t *csync_vio_open(const char *uri, int flags, mode_t mode);
csync_vio_handle_t *csync_vio_creat(const char *uri, mode_t mode);
int csync_vio_close(csync_vio_handle_t *handle);
ssize_t csync_vio_read(csync_vio_handle_t *fhandle, void *buf, size_t count);
ssize_t csync_vio_write(csync_vio_handle_t *fhandle, const void *buf, size_t count);
off_t csync_vio_seek(csync_vio_handle_t *fhandle, off_t offset, int whence);

csync_vio_handle_t *csync_vio_opendir(const char *name);
int csync_vio_closedir(csync_vio_handle_t *dhandle);
csync_vio_file_stat_t *csync_vio_readdir(csync_vio_handle_t *dhandle);

int csync_vio_mkdir(const char *uri, mode_t mode);
int csync_vio_rmdir(const char *uri);

int csync_vio_stat(const char *uri, csync_vio_file_stat_t *buf);
int csync_vio_rename(const char *olduri, const char *newuri);
int csync_vio_unlink(const char *uri);

int csync_vio_chmod(const char *uri, mode_t mode);
int csync_vio_chown(const char *uri, uid_t owner, gid_t group);

int csync_vio_utimes(const char *uri, const struct timeval times[2]);

#endif /* _CSYNC_VIO_H */
