/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2012      by Andreas Schneider <asn@cryptomilk.org>
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
 */

#ifndef _CSYNC_MISC_H
#define _CSYNC_MISC_H

#include "config.h"
#include "csync.h"

#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
#define
/* Steal this define to make csync_exclude compile. Note that if fnmatch
 * is not defined it's probably Win32 which uses a different implementation
 * than fmmatch anyway, which does not care for flags.
 **/
#define FNM_PATHNAME    (1 << 0) /* No wildcard can ever match `/'.  */
#endif

char *csync_get_user_home_dir(void);
char *csync_get_local_username(void);

int csync_fnmatch(__const char *__pattern, __const char *__name, int __flags);

/**
 * @brief csync_errno_to_csync_error - errno to csync error code
 *
 * This function tries to convert the value of the current set errno
 * to a csync error code.
 *
 * @return the corresponding csync error code.
 */
CSYNC_ERROR_CODE csync_errno_to_csync_error(CSYNC_ERROR_CODE default_err);

struct csync_hbf_info_s {
    int start_id;
    int transfer_id;
};
typedef struct csync_hbf_info_s csync_hbf_info_t;

typedef struct {
    int64_t file_count;
    int64_t current_file_no;
    int64_t byte_sum;
    int64_t byte_current;
} csync_overall_progress_t;

#endif /* _CSYNC_MISC_H */
