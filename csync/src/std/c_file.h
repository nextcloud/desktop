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
 * @file c_file.h
 *
 * @brief Interface of the cynapses libc file function
 *
 * @defgroup cynFileInternals cynapses libc file functions
 * @ingroup cynLibraryAPI
 *
 * @{
 */

#ifndef _C_FILE_H
#define _C_FILE_H

#include <sys/types.h>
#include <stdio.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE (16 * 1024)
#endif

#ifdef _WIN32
/**
 * @brief Check if a path is a link.
 *
 * @param path  The path to check.
 *
 * @return 1 if the path is a symbolic link, 0 if the path doesn't
 *         exist or is something else.
 */
int c_islink(const char *path);
#endif

/**
 * @brief Check if a path is a regular file or a link.
 *
 * @param path  The path to check.
 *
 * @return 1 if the path is a file, 0 if the path doesn't exist, is
 *         something else or can't be accessed.
 */
int c_isfile(const char *path);

/**
 * @brief copy a file from source to destination.
 *
 * @param src    Path to the source file
 * @param dst    Path to the destination file
 * @param mode   File creation mode of the destination. If mode is 0 then the
 *               mode from the source will be used.
 *
 * @return       0 on success, less than 0 on error with errno set.
 *               EISDIR if src or dst is a file.
 */
int c_copy(const char *src, const char *dst, mode_t mode);

/**
 * @brief Compare the content of two files byte by byte.
 * @param f1     Path of file 1
 * @param f2     Path of file 2
 *
 * @return       0 if the files differ, 1 if the files are equal or -1 on
 *               error with errno set.
 */
int c_compare_file( const char *f1, const char *f2 );

/**
 * @brief move a file from source to destination.
 *
 * @param src    Path to the source file
 * @param dst    Path to the destination file
 *
 * @return       0 on success, less than 0 on error with errno set.
 */
int c_rename( const char *src, const char *dst );

/**
 * }@
 */
#endif /* _C_FILE_H */

