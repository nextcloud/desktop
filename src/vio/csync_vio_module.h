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

#ifndef _CSYNC_VIO_MODULE_H
#define _CSYNC_VIO_MODULE_H

#include "vio/csync_vio_method.h"

extern csync_vio_method_t *vio_module_init(const char *method_name,
    const char *args, csync_auth_callback cb, void *userdata);
extern void vio_module_shutdown(csync_vio_method_t *method);

extern int csync_vio_getfd(csync_vio_handle_t *hnd);


#endif /* _CSYNC_VIO_MODULE_H */
