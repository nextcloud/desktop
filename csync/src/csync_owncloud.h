/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2011      by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012      by Klaas Freitag <freitag@owncloud.com>
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
#ifndef CSYNC_OWNCLOUD_H
#define CSYNC_OWNCLOUD_H

#include "csync.h"
#include "vio/csync_vio.h"

// Public API used by csync
int owncloud_commit(CSYNC* ctx);
void owncloud_destroy(CSYNC* ctx);
char *owncloud_error_string(CSYNC* ctx);
int owncloud_set_property(CSYNC* ctx, const char *key, void *data);
void owncloud_init(CSYNC* ctx);

int dav_connect(CSYNC* ctx,  const char *base_url);

#endif /* CSYNC_OWNCLOUD_H */
