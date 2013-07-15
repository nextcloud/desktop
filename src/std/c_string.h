/*
 * cynapses libc functions
 *
 * Copyright (c) 2008 by Andreas Schneider <mail@cynapses.org>
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
 * vim: ts=2 sw=2 et cindent
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
 * @breif Replace a string with another string in a source string.
 *
 * @param src      String to search for pattern.
 *
 * @param pattern  Pattern to search for in the source string.
 *
 * @param repl     The string which which should replace pattern if found.
 *
 * @return  Return a pointer to the source string.
 */
char *c_strreplace(char *src, const char *pattern, const char *repl);

/**
 * @brief Uppercase a string.
 *
 * @param  str     The String to uppercase.
 *
 * @return The malloced uppered string or NULL on error.
 */
char *c_uppercase(const char* str);

/**
 * @brief Lowercase a string.
 *
 * @param  str     The String to lowercase.
 *
 * @return The malloced lowered string or NULL on error.
 */
char *c_lowercase(const char* str);

/**
 * @brief Convert a multibyte string to utf8 (Win32).
 *
 * @param  str     The multibyte encoded string to convert
 *
 * @return The malloced converted string or NULL on error.
 */
 char*   c_utf8(const _TCHAR *str);

/**
 * @brief Convert a utf8 encoded string to multibyte (Win32).
 *
 * @param  str     The utf8 string to convert.
 *
 * @return The malloced converted multibyte string or NULL on error.
 */
_TCHAR* c_multibyte(const char *wstr);

#if defined(_WIN32) || defined(WITH_ICONV)
/**
 * @brief Free buffer malloced by c_multibyte.
 *
 * @param  buf     The buffer to free.
 */

#define c_free_multibyte(x) SAFE_FREE(x)

/**
 * @brief Free buffer malloced by c_utf8.
 *
 * @param  buf     The buffer to free.
 *
 */
#define c_free_utf8(x) SAFE_FREE(x)
#else
#define c_free_multibyte(x) (void)x
#define c_free_utf8(x) (void)x
#endif


/**
 * }@
 */
#endif /* _C_STR_H */

