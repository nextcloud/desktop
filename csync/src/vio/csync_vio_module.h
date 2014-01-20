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

#ifndef _CSYNC_VIO_MODULE_H
#define _CSYNC_VIO_MODULE_H

#include "vio/csync_vio_method.h"

extern csync_vio_method_t *vio_module_init(const char *method_name,
    const char *args, csync_auth_callback cb, void *userdata);
extern void vio_module_shutdown(csync_vio_method_t *method);

extern int csync_vio_getfd(csync_vio_handle_t *hnd);


#endif /* _CSYNC_VIO_MODULE_H */
