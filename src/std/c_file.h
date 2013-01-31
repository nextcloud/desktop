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
 * vim: ft=c.doxygen ts=2 sw=2 et cindent
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

/**
 * @brief Check if a path is a regular file or a link.
 *
 * @param path  The path to check.
 *
 * @return 1 if the path is a file, 0 if the path doesn't exist, is a
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

