/*
 * libcsync -- a library to sync a directory with another
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

#ifndef _CSYNC_EXCLUDE_H
#define _CSYNC_EXCLUDE_H

#include "ocsynclib.h"

enum csync_exclude_type_e {
  CSYNC_NOT_EXCLUDED   = 0,
  CSYNC_FILE_SILENTLY_EXCLUDED,
  CSYNC_FILE_EXCLUDE_AND_REMOVE,
  CSYNC_FILE_EXCLUDE_LIST,
  CSYNC_FILE_EXCLUDE_INVALID_CHAR,
  CSYNC_FILE_EXCLUDE_TRAILING_SPACE,
  CSYNC_FILE_EXCLUDE_LONG_FILENAME,
  CSYNC_FILE_EXCLUDE_HIDDEN,
  CSYNC_FILE_EXCLUDE_STAT_FAILED,
  CSYNC_FILE_EXCLUDE_CONFLICT
};
typedef enum csync_exclude_type_e CSYNC_EXCLUDE_TYPE;

#ifdef WITH_TESTING
int OCSYNC_EXPORT _csync_exclude_add(c_strlist_t **inList, const char *string);
#endif

/**
 * @brief Load exclude list
 *
 * @param ctx    The context of the synchronizer.
 * @param fname  The filename to load.
 *
 * @return  0 on success, -1 if an error occurred with errno set.
 */
int OCSYNC_EXPORT csync_exclude_load(const char *fname, c_strlist_t **list);

/**
 * @brief Check if the given path should be excluded in a traversal situation.
 *
 * It does only part of the work that csync_excluded does because it's assumed
 * that all leading directories have been run through csync_excluded_traversal()
 * before. This can be significantly faster.
 *
 * That means for '/foo/bar/file' only ('/foo/bar/file', 'file') is checked
 * against the exclude patterns.
 *
 * @param ctx   The synchronizer context.
 * @param path  The patch to check.
 *
 * @return  2 if excluded and needs cleanup, 1 if excluded, 0 if not.
 */
CSYNC_EXCLUDE_TYPE csync_excluded_traversal(c_strlist_t *excludes, const char *path, int filetype);

/**
 * @brief csync_excluded_no_ctx
 * @param excludes
 * @param path
 * @param filetype
 * @return
 */
CSYNC_EXCLUDE_TYPE OCSYNC_EXPORT csync_excluded_no_ctx(c_strlist_t *excludes, const char *path, int filetype);
#endif /* _CSYNC_EXCLUDE_H */

/**
 * @brief Checks if filename is considered reserved by Windows
 * @param file_name filename
 * @return true if file is reserved, false otherwise
 */
bool csync_is_windows_reserved_word(const char *file_name);


/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
