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
 * @file c_dir.h
 *
 * @brief Interface of the cynapses libc directory function
 *
 * @defgroup cynDirInternals cynapses libc directory functions
 * @ingroup cynLibraryAPI
 *
 * @{
 */

#ifndef _C_DIR_H
#define _C_DIR_H

#include <sys/types.h>


/**
 * @brief Create parent directories as needed.
 *
 * The newly created directory will be owned by the effective user ID of the
 * process.
 *
 * @param path      The path to the directory to create.
 *
 * @param mode      Specifies  the  permissions to use. It is modified
 *                  by the process's umask in the usual way: the
 *                  permissions of the created file are (mode & ~umask).
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EACCES The parent directory does not allow write
 *                    permission to the process, or one of the directories
 *                  - ENOTDIR if durl is not a directory
 *                  - EINVAL NULL durl passed or smbc_init not called.
 *                  - ENOMEM Insufficient memory was available.
 *
 * @see mkdir()
 */
int c_mkdirs(const char *path, mode_t mode);

/**
 * @brief Remove the directory and subdirectories including the content.
 *
 * This removes all directories and files recursivly.
 *
 * @param  dir      The directory to remove recusively.
 *
 * @return          0 on success, < 0 on error with errno set.
 */
int c_rmdirs(const char *dir);

/**
 * @brief Check if a path is a directory.
 *
 * @param path  The path to check.
 *
 * @return 1 if the path is a directory, 0 if the path doesn't exist, is a
 *         file or can't be accessed.
 */
int c_isdir(const char *path);

/**
 * }@
 */
#endif /* _CDIR_H */

