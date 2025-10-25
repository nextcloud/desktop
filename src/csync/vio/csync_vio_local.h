/*
 * libcsync -- a library to sync a directory with another
 *
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-FileCopyrightText: 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _CSYNC_VIO_LOCAL_H
#define _CSYNC_VIO_LOCAL_H

#include <QString>

struct csync_vio_handle_t;
namespace OCC {
class Vfs;
}

csync_vio_handle_t OCSYNC_EXPORT *csync_vio_local_opendir(const QString &name);
int OCSYNC_EXPORT csync_vio_local_closedir(csync_vio_handle_t *dhandle);
std::unique_ptr<csync_file_stat_t> OCSYNC_EXPORT csync_vio_local_readdir(csync_vio_handle_t *dhandle, OCC::Vfs *vfs, bool checkPermissionsValidity);

int OCSYNC_EXPORT csync_vio_local_stat(const QString &uri, csync_file_stat_t *buf, bool checkPermissionsValidity);

#endif /* _CSYNC_VIO_LOCAL_H */
