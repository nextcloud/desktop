/*
 * libcsync -- a library to sync a directory with another
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

#ifndef _CSYNC_MISC_H
#define _CSYNC_MISC_H

#include <config.h>
#include <csync.h>

#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
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
 * @brief csync_errno_to_status - errno to csync status code
 *
 * This function tries to convert the value of the current set errno
 * to a csync status code.
 *
 * @return the corresponding csync error code.
 */
CSYNC_STATUS csync_errno_to_status(int error, CSYNC_STATUS default_status);

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


char *csync_normalize_etag(const char *);

#endif /* _CSYNC_MISC_H */
