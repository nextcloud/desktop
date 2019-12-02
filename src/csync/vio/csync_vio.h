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
#include "csync.h"
#include "csync_private.h"

typedef struct fhandle_s {
  int fd;
} fhandle_t;

csync_vio_handle_t *csync_vio_opendir(CSYNC *ctx, const char *name);
int csync_vio_closedir(CSYNC *ctx, csync_vio_handle_t *dhandle);
std::unique_ptr<csync_file_stat_t> csync_vio_readdir(CSYNC *ctx, csync_vio_handle_t *dhandle);

char *csync_vio_get_status_string(CSYNC *ctx);


#endif /* _CSYNC_VIO_H */
