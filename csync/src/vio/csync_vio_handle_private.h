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

#ifndef _CSYNC_VIO_HANDLE_PRIVATE_H
#define _CSYNC_VIO_HANDLE_PRIVATE_H

#include "vio/csync_vio_handle.h"

struct csync_vio_handle_s {
  char *uri;

  csync_vio_method_handle_t *method_handle;
};

csync_vio_handle_t *csync_vio_handle_new(const char *uri, csync_vio_method_handle_t *method_handle);
void csync_vio_handle_destroy(csync_vio_handle_t *handle);

#endif /* _CSYNC_VIO_HANDLE_PRIVATE_H */
