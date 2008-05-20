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

#ifndef _CSYNC_UPDATE_H
#define _CSYNC_UPDATE_H

#include "csync.h"
#include "vio/csync_vio_file_stat.h"

/**
 * Types for files
 */
enum csync_ftw_flags_e {
  CSYNC_FTW_FLAG_FILE,		/* Regular file.  */
  CSYNC_FTW_FLAG_DIR,		/* Directory.  */
  CSYNC_FTW_FLAG_DNR,		/* Unreadable directory.  */
  CSYNC_FTW_FLAG_NSTAT,		/* Unstatable file.  */
  CSYNC_FTW_FLAG_SLINK,		/* Symbolic link.  */
  /* These flags are only passed from the `nftw' function.  */
  CSYNC_FTW_FLAG_DP,		/* Directory, all subdirs have been visited. */
  CSYNC_FTW_FLAG_SLN		/* Symbolic link naming non-existing file.  */
};

typedef int (*csync_walker_fn) (CSYNC *ctx, const char *file, const csync_vio_file_stat_t *fs, enum csync_ftw_flags_e flag);

int csync_walker(CSYNC *ctx, const char *file, const csync_vio_file_stat_t *fs, enum csync_ftw_flags_e flag);
int csync_ftw(CSYNC *ctx, const char *uri, csync_walker_fn fn, unsigned int depth);

#endif /* _CSYNC_UPDATE_H */
