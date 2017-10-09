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
#ifndef _C_UTF8_H
#define _C_UTF8_H

#include "c_private.h"
#include "c_macro.h"

#ifdef __cplusplus
#include <QByteArray>

/**
 * @brief Convert a platform locale string to utf8.
 *
 * This function is part of the multi platform abstraction of basic file
 * operations to handle various platform encoding correctly.
 *
 * Instead of using the standard file operations the multi platform aliases
 * defined in c_private.h have to be used instead.
 *
 * To convert path names returned by these functions to the internally used
 * utf8 format this function has to be used.
 *
 * @param  str     The multibyte encoded string to convert
 *
 * @return The converted string or a null QByteArray on error.
 *
 * @see c_free_locale_string()
 * @see c_utf8_to_locale()
 *
 */
 QByteArray c_utf8_from_locale(const mbchar_t *str);

extern "C" {

#endif // __cplusplus

/**
 * @brief Convert a utf8 encoded string to platform specific locale.
 *
 * This function is part of the multi platform abstraction of basic file
 * operations to handle various platform encoding correctly.
 *
 * Instead of using the standard file operations the multi platform aliases
 * defined in c_private.h have to be used instead.
 *
 * To convert strings as input for the cross platform functions from the
 * internally used utf8 format, this function has to be used.
 * The returned string has to be freed by c_free_locale_string(). On some
 * platforms this method allocates memory and on others not but it has never
 * sto be cared about.
 *
 * If the string to convert is a path, consider using c_utf8_path_to_locale().
 *
 * @param  str     The utf8 string to convert.
 *
 * @return The malloced converted multibyte string or NULL on error.
 *
 * @see c_free_locale_string()
 * @see c_utf8_from_locale()
 *
 */
mbchar_t* c_utf8_string_to_locale(const char *wstr);

/**
 * @brief c_utf8_path_to_locale converts a unixoid path to the locale aware format
 *
 * On windows, it converts to UNC and multibyte.
 * On Mac, it converts to the correct utf8 using iconv.
 * On Linux, it returns utf8
 *
 * @param str The path to convert
 *
 * @return a pointer to the converted string. Caller has to free it using the
 *         function c_free_locale_string.
 */
mbchar_t* c_utf8_path_to_locale(const char *str);

/**
 * @brief Free buffer malloced by c_utf8_to_locale().
 *
 * This function is part of the multi platform abstraction of basic file
 * operations to handle various platform encoding correctly.
 *
 * Instead of using the standard file operations the multi platform aliases
 * defined in c_private.h have to be used instead.
 *
 * This function frees the memory that was allocated by a previous call to
 * c_utf8_to_locale().
 *
 * @param  buf     The buffer to free.
 *
 * @see c_utf8_from_locale(), c_utf8_to_locale()
 *
 */
#define c_free_locale_string(x) SAFE_FREE(x)


/**
 * }@
 */

#ifdef __cplusplus
}
#endif

#endif /* _C_UTF8_H */
