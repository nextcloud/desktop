/*
 * cynapses libc functions
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>
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

#ifndef _CSYNC_MACROS_H
#define _CSYNC_MACROS_H

#include <stdlib.h>
#include <string.h>

/* How many elements there are in a static array */
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

/* Some special custom errno values to report bugs properly. The BASE value
 * should always be larger than the highest system errno. */
#define CSYNC_CUSTOM_ERRNO_BASE 10000

#define ERRNO_WRONG_CONTENT          CSYNC_CUSTOM_ERRNO_BASE+11
#define ERRNO_SERVICE_UNAVAILABLE    CSYNC_CUSTOM_ERRNO_BASE+14
#define ERRNO_STORAGE_UNAVAILABLE    CSYNC_CUSTOM_ERRNO_BASE+17
#define ERRNO_FORBIDDEN              CSYNC_CUSTOM_ERRNO_BASE+18

#endif /* _CSYNC_MACROS_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
