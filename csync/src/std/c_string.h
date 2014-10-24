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

#include "c_private.h"
#include "c_macro.h"

#include <stdlib.h>

struct c_strlist_s; typedef struct c_strlist_s c_strlist_t;

/**
 * @brief Structure for a stringlist
 *
 * Using a for loop you can access the strings saved in the vector.
 *
 * c_strlist_t strlist;
 * int i;
 * for (i = 0; i < strlist->count; i++) {
 *   printf("value: %s", strlist->vector[i];
 * }
 */
struct c_strlist_s {
  /** The string vector */
  char **vector;
  /** The count of the strings saved in the vector */
  size_t count;
  /** Size of strings allocated */
  size_t size;
};

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
 * @brief Create a new stringlist.
 *
 * @param size  Size to allocate.
 *
 * @return  Pointer to the newly allocated stringlist. NULL if an error occured.
 */
c_strlist_t *c_strlist_new(size_t size);

/**
 * @brief Expand the stringlist
 *
 * @param strlist  Stringlist to expand
 * @param size     New size of the strlinglist to expand
 *
 * @return  Pointer to the expanded stringlist. NULL if an error occured.
 */
c_strlist_t *c_strlist_expand(c_strlist_t *strlist, size_t size);

/**
 * @brief  Add a string to the stringlist.
 *
 * Duplicates the string and stores it in the stringlist.
 *
 * @param strlist  Stringlist to add the string.
 * @param string   String to add.
 *
 * @return  0 on success, less than 0 and errno set if an error occured.
 *          ENOBUFS if the list is full.
 */
int c_strlist_add(c_strlist_t *strlist, const char *string);

/**
 * @brief Removes all strings from the list.
 *
 * Frees the strings.
 *
 * @param strlist  Stringlist to clear
 */
void c_strlist_clear(c_strlist_t *strlist);

/**
 * @brief Destroy the memory of the stringlist.
 *
 * Frees the strings and the stringlist.
 *
 * @param strlist  Stringlist to destroy
 */
void c_strlist_destroy(c_strlist_t *strlist);

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
 * utf8 format this function has to be used. The returned string has to
 * be freed by c_free_locale_string(). On some platforms this method allocates
 * memory and on others not but it has never to be cared about.
 *
 * @param  str     The multibyte encoded string to convert
 *
 * @return The malloced converted string or NULL on error.
 *
 * @see c_free_locale_string()
 * @see c_utf8_to_locale()
 *
 */
 char*   c_utf8_from_locale(const mbchar_t *str);

/**
 * @brief Convert a utf8 encoded string to platform specific locale.
 *
 * This function is part of the multi platform abstraction of basic file
 * operations to handle various platform encoding correctly.
 *
 * Instead of using the standard file operations the multi platform aliases
 * defined in c_private.h have to be used instead.
 *
 * To convert path names as input for the cross platform functions from the
 * internally used utf8 format, this function has to be used.
 * The returned string has to be freed by c_free_locale_string(). On some
 * platforms this method allocates memory and on others not but it has never
 * sto be cared about.
 *
 * @param  str     The utf8 string to convert.
 *
 * @return The malloced converted multibyte string or NULL on error.
 *
 * @see c_free_locale_string()
 * @see c_utf8_from_locale()
 *
 */
mbchar_t* c_utf8_to_locale(const char *wstr);

#if defined(_WIN32) || defined(WITH_ICONV)

/**
 * @brief Free buffer malloced by c_utf8_from_locale or c_utf8_to_locale().
 *
 * This function is part of the multi platform abstraction of basic file
 * operations to handle various platform encoding correctly.
 *
 * Instead of using the standard file operations the multi platform aliases
 * defined in c_private.h have to be used instead.
 *
 * This function frees the memory that was allocated by a previous call to
 * c_utf8_to_locale() or c_utf8_from_locale().
 *
 * @param  buf     The buffer to free.
 *
 * @see c_utf8_from_locale(), c_utf8_to_locale()
 *
 */
#define c_free_locale_string(x) SAFE_FREE(x)
#else
#define c_free_locale_string(x) (void)x
#endif

/**
 * }@
 */
#endif /* _C_STR_H */

