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

/**
 * @file c_path.h
 *
 * @brief Interface of the cynapses libc path functions
 *
 * @defgroup cynPathInternals cynapses libc path functions
 * @ingroup cynLibraryAPI
 *
 * @{
 */

#ifndef _C_PATH_H
#define _C_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "c_macro.h"
#include "c_private.h"

/**
 * @brief Parse directory component.
 *
 * dirname breaks a null-terminated pathname string into a directory component.
 * In the usual case, c_dirname() returns the string up to, but not including,
 * the final '/'. Trailing '/' characters are  not  counted as part of the
 * pathname. The caller must free the memory.
 *
 * @param path  The path to parse.
 *
 * @return  The dirname of path or NULL if we can't allocate memory. If path
 *          does not contain a slash, c_dirname() returns the string ".".  If
 *          path is the string "/", it returns the string "/". If path is
 *          NULL or an empty string, "." is returned.
 */
char *c_dirname(const char *path);

/**
 * @brief basename - parse filename component.
 *
 * basename breaks a null-terminated pathname string into a filename component.
 * c_basename() returns the component following the final '/'.  Trailing '/'
 * characters are not counted as part of the pathname.
 *
 * @param path The path to parse.
 *
 * @return  The filename of path or NULL if we can't allocate memory. If path
 *          is a the string "/", basename returns the string "/". If path is
 *          NULL or an empty string, "." is returned.
 */
char *c_basename (const char *path);

/**
 * @brief parse a uri and split it into components.
 *
 * parse_uri parses an uri in the format
 *
 * [<scheme>:][//[<user>[:<password>]@]<host>[:<port>]]/[<path>]
 *
 * into its compoments. If you only want a special component,
 * pass NULL for all other components. All components will be allocated if they have
 * been found.
 *
 * @param uri       The uri to parse.
 * @param scheme    String for the scheme component
 * @param user      String for the username component
 * @param passwd    String for the password component
 * @param host      String for the password component
 * @param port      Integer for the port
 * @param path      String for the path component with a leading slash.
 *
 * @return  0 on success, < 0 on error.
 */
int c_parse_uri(const char *uri, char **scheme, char **user, char **passwd,
    char **host, unsigned int *port, char **path);

/**
 * @brief Parts of a path.
 *
 * @param directory '\0' terminated path including the final '/'
 *
 * @param filename '\0' terminated string
 *
 * @param extension '\0' terminated string
 *
 */
typedef struct
{
    char * directory;
    char * filename;
    char * extension;
} C_PATHINFO;

/**
 * @brief c_path_to_UNC converts a unixoid path to UNC format.
 *
 * It converts the '/' to '\' and prepends \\?\ to the path.
 *
 * A proper windows path has to have a drive letter, otherwise it is not
 * valid UNC.
 *
 * @param str The path to convert
 *
 * @return a pointer to the converted string. Caller has to free it.
 */
const char *c_path_to_UNC(const char *str);

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
 * }@
 */

#ifdef __cplusplus
}
#endif

#endif /* _C_PATH_H */
