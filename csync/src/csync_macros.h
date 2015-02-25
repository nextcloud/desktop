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

#define ERRNO_GENERAL_ERROR          CSYNC_CUSTOM_ERRNO_BASE+2
#define ERRNO_LOOKUP_ERROR           CSYNC_CUSTOM_ERRNO_BASE+3
#define ERRNO_USER_UNKNOWN_ON_SERVER CSYNC_CUSTOM_ERRNO_BASE+4
#define ERRNO_PROXY_AUTH             CSYNC_CUSTOM_ERRNO_BASE+5
#define ERRNO_CONNECT                CSYNC_CUSTOM_ERRNO_BASE+6
#define ERRNO_TIMEOUT                CSYNC_CUSTOM_ERRNO_BASE+7
#define ERRNO_PRECONDITION           CSYNC_CUSTOM_ERRNO_BASE+8
#define ERRNO_RETRY                  CSYNC_CUSTOM_ERRNO_BASE+9
#define ERRNO_REDIRECT               CSYNC_CUSTOM_ERRNO_BASE+10
#define ERRNO_WRONG_CONTENT          CSYNC_CUSTOM_ERRNO_BASE+11
#define ERRNO_ERROR_STRING           CSYNC_CUSTOM_ERRNO_BASE+13
#define ERRNO_SERVICE_UNAVAILABLE    CSYNC_CUSTOM_ERRNO_BASE+14
#define ERRNO_USER_ABORT             CSYNC_CUSTOM_ERRNO_BASE+16
#define ERRNO_STORAGE_UNAVAILABLE    CSYNC_CUSTOM_ERRNO_BASE+17

#endif /* _CSYNC_MACROS_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
