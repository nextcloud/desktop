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

#ifndef _CSYNC_VIO_HANDLE_PRIVATE_H
#define _CSYNC_VIO_HANDLE_PRIVATE_H

struct csync_vio_handle_s {
  char *uri;

  csync_vio_method_handle_t *method_handle;
};

csync_vio_handle_t *csync_vio_handle_new(char *uri, csync_vio_method_handle_t *method_handle);
void csync_vio_handle_destroy(csync_vio_handle_t *handle);

#endif /* _CSYNC_VIO_HANDLE_PRIVATE_H */
