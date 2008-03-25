/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008      by Andreas Schneider <mail@cynapses.org>
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

#ifndef _CSYNC_EXCLUDE_H
#define _CSYNC_EXCLUDE_H

/**
 * @brief Load exclude list
 *
 * @param ctx    The context of the synchronizer.
 * @param fname  The filename to load.
 *
 * @return  0 on success, -1 if an error occured with errno set.
 */
int csync_exclude_load(CSYNC *ctx, const char *fname);

/**
 * @brief Destroy the exclude list in memory.
 *
 * @param ctx   The synchronizer context.
 */
void csync_exclude_destroy(CSYNC *ctx);

/**
 * @brief Check if the given path should be excluded.
 *
 * @param ctx   The synchronizer context.
 * @param path  The patch to check.
 *
 * @return  1 if excluded, 0 if not.
 */
int csync_excluded(CSYNC *ctx, const char *path);

#endif /* _CSYNC_EXCLUDE_H */
