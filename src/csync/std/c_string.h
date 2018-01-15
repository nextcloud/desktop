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

/**
 * @file c_string.h
 *
 * @brief Interface of the cynapses string implementations
 *
 * @defgroup cynStringInternals cynapses libc string functions
 * @ingroup cynLibraryAPI
 *
 * @{
 */
#ifndef _C_STR_H
#define _C_STR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "c_private.h"
#include "c_macro.h"

#include <stdlib.h>

/**
 * @brief Compare to strings case insensitively.
 *
 * @param a  First string to compare.
 * @param b  Second string to compare.
 * @param n  Max comparison length.
 *
 * @return see strncasecmp
 */
int c_strncasecmp(const char *a, const char *b, size_t n);

/**
 * @brief Compare to strings if they are equal.
 *
 * @param a  First string to compare.
 * @param b  Second string to compare.
 *
 * @return  1 if they are equal, 0 if not.
 */
int c_streq(const char *a, const char *b);


/**
 * }@
 */

#ifdef __cplusplus
}
#endif

#endif /* _C_STR_H */

