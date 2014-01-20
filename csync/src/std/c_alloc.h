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
 * @file c_alloc.h
 *
 * @brief Interface of the cynapses libc alloc function
 *
 * @defgroup cynLibraryAPI cynapses libc API (internal)
 *
 * @defgroup cynAllocInternals cynapses libc alloc functions
 * @ingroup cynLibraryAPI
 *
 * @{
 */

#ifndef _C_ALLOC_H
#define _C_ALLOC_H

#include <stdlib.h>

#include "c_macro.h"

/**
 * @brief Allocates memory for an array.
 *
 * Allocates memory for an array of <count> elements of <size> bytes each and
 * returns a pointer to the allocated memory. The memory is set to zero.
 *
 * @param count   Amount of elements to allocate
 * @param size    Size in bytes of each element to allocate.
 *
 * @return A unique pointer value that can later be successfully passed to
 *         free(). If size or count is 0, NULL will be returned.
 */
void *c_calloc(size_t count, size_t size);

/**
 * @brief Allocates memory for an array.
 *
 * Allocates <size> bytes of memory. The memory is set to zero.
 *
 * @param size    Size in bytes to allocate.
 *
 * @return A unique pointer value that can later be successfully passed to
 *         free(). If size or count is 0, NULL will be returned.
 */
void *c_malloc(size_t size);

/**
 * @brief Changes the size of the memory block pointed to.
 *
 * Newly allocated memory will be uninitialized.
 *
 * @param ptr   Pointer to the memory which should be resized.
 * @param size  Value to resize.
 *
 * @return If ptr is NULL, the call is equivalent to c_malloc(size); if size
 *         is equal to zero, the call is equivalent to free(ptr). Unless ptr
 *         is NULL, it must have been returned by an earlier call to
 *         c_malloc(), c_calloc() or c_realloc(). If the area pointed to was
 *         moved, a free(ptr) is done.
 */
void *c_realloc(void *ptr, size_t size);

/**
 * @brief Duplicate a string.
 *
 * The function returns a pointer to a newly allocated string which is a
 * duplicate of the string str.
 *
 * @param str   String to duplicate.
 *
 * @return Returns a pointer to the duplicated string, or NULL if insufficient
 * memory was available.
 *
 */
char *c_strdup(const char *str);

/**
 * @brief Duplicate a string.
 *
 * The function returns a pointer to a newly allocated string which is a
 * duplicate of the string str of size bytes.
 *
 * @param str   String to duplicate.
 *
 * @param size  Size of the string to duplicate.
 *
 * @return Returns a pointer to the duplicated string, or NULL if insufficient
 * memory was available. A terminating null byte '\0' is added.
 *
 */
char *c_strndup(const char *str, size_t size);

/**
 * }@
 */
#endif /* _C_ALLOC_H */
