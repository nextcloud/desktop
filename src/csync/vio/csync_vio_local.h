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

#ifndef _CSYNC_VIO_LOCAL_H
#define _CSYNC_VIO_LOCAL_H

#include "ocsynclib.h"

#include <QString>

struct csync_vio_handle_t;
namespace OCC {
class Vfs;
}

csync_vio_handle_t OCSYNC_EXPORT *csync_vio_local_opendir(const QString &name);
int OCSYNC_EXPORT csync_vio_local_closedir(csync_vio_handle_t *dhandle);
std::unique_ptr<csync_file_stat_t> OCSYNC_EXPORT csync_vio_local_readdir(csync_vio_handle_t *dhandle, OCC::Vfs *vfs);

int OCSYNC_EXPORT csync_vio_local_stat(const QString &uri, csync_file_stat_t *buf);

#endif /* _CSYNC_VIO_LOCAL_H */
